#include "ert.hh"
#include "esys.hh"
#include "enat.hh"

namespace espresso {

void Panic(const char* msg) {
    throw PanicException{msg};
}

PanicException::PanicException(const char* msg) {
    this->message = msg;
}

const char* PanicException::what() const noexcept {
    return this->message;
}

ThrowException::ThrowException(Integer stackIndex) {
    this->stackIndex = stackIndex;
}

const char* ThrowException::what() const noexcept {
    return "Espresso Exception";
}

void Runtime::Init(System* system) {
    this->system = system;
    this->heap = nullptr;
    this->globals = nullptr;
    
    this->stack.Init(this);
    this->frames.Init(this);

    globals = this->NewMap();

    CallFrame* initialFrame = this->frames.Push(this);
    initialFrame->Init(Integer{0}, Integer{4});
    // null out the initial stack
    this->stack.Push(this)->SetNil();
    this->stack.Push(this)->SetNil();
    this->stack.Push(this)->SetNil();
    this->stack.Push(this)->SetNil();

    // load natives
    Local(Integer{0})->SetNativeFunction(NewNativeFunction(Integer{1}, Integer{2}, espresso::native::RegisterNatives));
    Invoke(Integer{0}, Integer{1});
}

void Runtime::DeInit() {
    this->stack.DeInit(this);
    this->frames.DeInit(this);
}

System* Runtime::GetSystem() {
    return this->system;
}

void Runtime::Invoke(Integer localBase, Integer argumentCount) {

    ValueType fnType = this->Local(localBase)->GetType();

    if (!(fnType == ValueType::Function || fnType == ValueType::NativeFunction)) {
        this->Local(Integer{0})->SetString(this->NewString("Illegal cast to function"));
        this->Throw(Integer{0});
        return;
    }

    Integer localCount = (fnType == ValueType::Function)
        ? Local(localBase)->GetFunction(this)->GetLocalCount()
        : Local(localBase)->GetNativeFunction(this)->GetLocalCount();

    Integer arity = (fnType == ValueType::Function)
        ? Local(localBase)->GetFunction(this)->GetArity()
        : Local(localBase)->GetNativeFunction(this)->GetArity();

    // Too Many / Too Few args passed
    if (arity.Unwrap() != argumentCount.Unwrap()) {
        Local(Integer{0})->SetString(NewString("Invalid arity"));
        Throw(Integer{0});
        return;
    }

    Integer absoluteBase = CurrentFrame()->AbsoluteIndex(localBase);
    frames.Push(this)->Init(absoluteBase, localCount);

    RuntimeDefer popFrameAtEnd(this, [](Runtime* rt) {
        rt->frames.Pop();
    });

    // prepare the new stack
    // 1. grow the stack until it's at least as large as the new top
    std::int64_t newAbsoluteStackSize = CurrentFrame()->AbsoluteIndex(localCount).Unwrap();
    while (stack.Length().Unwrap() < newAbsoluteStackSize) {
        stack.Push(this)->SetNil();
    }

    // 2. Nullify all memory that is not assigned yet
    std::int64_t startIndex = argumentCount.Unwrap();
    std::int64_t frameSize = CurrentFrame()->Size().Unwrap();
    for(std::int64_t i = startIndex; i < frameSize; i++) {
        Local(Integer{i})->SetNil();
    }

    // enter the actual function here

    if (fnType == ValueType::Function) {
        this->Interpret();

    } else /* val == ValueType::NativeFunction */ {
        NativeFunction* fn = Local(Integer{0})->GetNativeFunction(this);
        NativeFunction::Handle handle = fn->GetHandle();
        handle(this);
    }
}

template<typename T>
T* Runtime::New(Integer count) {
    // TODO: size checking
    // TODO: allocation tracking
    std::int64_t size = count.Unwrap() * sizeof(T);
    T* result = static_cast<T*>(this->system->ReAllocate(nullptr, 0, size));
    if (result == nullptr) {
        Panic("Out Of Memory");
        return nullptr;
    }
    return result;
}

template<typename T>
T* Runtime::ReAllocate(T* data, Integer prevCount, Integer newCount) {
    // TODO: size checking
    // TODO: allocation tracking
    std::int64_t prevSize = prevCount.Unwrap() * sizeof(T);
    std::int64_t newSize = newCount.Unwrap() * sizeof(T);
    T* result = static_cast<T*>(this->system->ReAllocate(data, prevSize, newSize));
    if (result == nullptr) {
        Panic("Out Of Memory");
        return nullptr;
    }
    return result;
}

template<typename T>
void Runtime::Free(T* pointer, Integer count) {
    // TODO: size checking
    this->system->ReAllocate(pointer, sizeof(T) * count.Unwrap(), 0);
}

RuntimeDefer::RuntimeDefer(Runtime* rt, RuntimeDefer::Handle fn)
: runtime{rt}
, handle{fn}
{}

RuntimeDefer::~RuntimeDefer() {
    this->handle(this->runtime);
}

Integer ThrowException::GetAbsoluteStackIndex() const {
    return this->stackIndex;
}

void* DefaultSystem::ReAllocate(
    void* pointer,
    std::size_t sizeBefore,
    std::size_t sizeAfter
) {
    (void)(sizeBefore);

    if (sizeAfter == 0) {
        std::free(pointer);
        return nullptr;
    } else {
        return std::realloc(pointer, sizeAfter);
    }
}

FILE* DefaultSystem::Open(const char* name, const char* mode) {
    return std::fopen(name, mode);
}

FILE* DefaultSystem::Stdout() {
    return stdout;
}

FILE* DefaultSystem::Stdin() {
    return stdin;
}

int DefaultSystem::Read(FILE* fp) {
    return std::fgetc(fp);
}

void DefaultSystem::Write(FILE* fp, const char* message, size_t size) {
    std::fwrite(message, sizeof(char), size, fp);
}

void DefaultSystem::Close(FILE* fp) {
    std::fclose(fp);
}

template<typename T>
void Vector<T>::Init(Runtime* rt) {
    InitWithCapacity(rt, Integer{0});
}

template<typename T>
void Vector<T>::InitWithCapacity(Runtime* rt, Integer capacity) {
    this->size = Integer{0};
    this->capacity = capacity;
    if (this->capacity.Unwrap() == 0) {
        this->data = nullptr;
    } else {
        this->data = rt->New<T>(this->capacity);
    }
}

template<typename T>
void Vector<T>::DeInit(Runtime* rt) {
    rt->Free<T>(this->data, this->capacity);
}

template<typename T>
T* Vector<T>::At(Integer index) const {
    std::int64_t val = index.Unwrap();
    if (val >= size.Unwrap() || val < 0) {
        Panic("IndexOutOfBounds");
        return nullptr;
    }
    return &this->data[val];
}

template<typename T>
T* Vector<T>::Push(Runtime* rt) {
    if (this->size.Unwrap() == this->capacity.Unwrap()) {
        Integer newCapacity = Integer{this->capacity.Unwrap() * 2};
        if (newCapacity.Unwrap() == 0) {
            newCapacity = Integer{8};
        }
        this->data = rt->ReAllocate<T>(this->data, this->capacity, newCapacity);
        this->capacity = newCapacity;
    }
    T* result = &this->data[this->size.Unwrap()];
    this->size = Integer{this->size.Unwrap() + 1};
    return result;
}

template<typename T>
void Vector<T>::Reserve(Runtime* rt, Integer capacity) {
    if (this->capacity.Unwrap() >= capacity.Unwrap()) {
        return;
    }
    this->data = rt->ReAllocate<T>(this->data, this->capacity, capacity);
    this->capacity = capacity;
}

template<typename T>
void Vector<T>::Pop() {
    if (this->size.Unwrap() <= 0) {
        Panic("Pop Underflow");
        return;
    }
    this->size = Integer{this->size.Unwrap() - 1};
}

template<typename T>
void Vector<T>::Truncate(Integer newSize) {
    if (this->size.Unwrap() < newSize.Unwrap()) {
        Panic("Truncate Underflow");
        return;
    }
    this->size = newSize;
}

template<typename T>
Integer Vector<T>::Length() const {
    return this->size;
}

template<typename T>
const T* Vector<T>::RawHeadPointer() const {
    return this->data;
}

CallFrame* Runtime::CurrentFrame() {
    Integer length = frames.Length();
    Integer last = Integer{length.Unwrap() - 1};
    return frames.At(last);
}

void CallFrame::Init(Integer stackBase, Integer argumentCount) {
    this->stackBase = stackBase;
    this->programCounter = Integer{0};
    this->stackSize = Integer{argumentCount.Unwrap()};
}

Integer CallFrame::AbsoluteIndex(Integer localIndex) const {
    return Integer{this->stackBase.Unwrap() + localIndex.Unwrap()};
}

Integer CallFrame::Size() const {
    return this->stackSize;
}

void Value::SetNil() {
    this->type = ValueType::Nil;
    this->as.integer = Integer{0};
}

void Runtime::LoadConstant(Integer dest, Integer constant) {
    Function* function = Local(Integer{0})->GetFunction(this);
    this->Local(dest)->Copy(function->ConstantAt(constant));
}

void Runtime::Return(Integer sourceIndex) {
    this->Local(Integer{0})->Copy(this->Local(sourceIndex));
}

void Runtime::Interpret() {
    while (true) {
        Function* function = Local(Integer{0})->GetFunction(this);
        ByteCode* byteCode = function->ByteCodeAt(CurrentFrame()->ProgramCounter());
        switch (byteCode->Type()) {
            case ByteCodeType::NoOp: {
                CurrentFrame()->AdvanceProgramCounter();
                break;
            }
            case ByteCodeType::Invoke: {
                CurrentFrame()->AdvanceProgramCounter();
                Integer arg1 = byteCode->SmallArgument1();
                Integer arg2 = byteCode->SmallArgument2();
                Invoke(arg1, arg2);
                break;
            }
            case ByteCodeType::LoadConstant: {
                CurrentFrame()->AdvanceProgramCounter();
                Integer arg1 = byteCode->SmallArgument1();
                Integer arg2 = byteCode->LargeArgument();
                LoadConstant(arg1, arg2);
                break;
            }
            case ByteCodeType::LoadGlobal: {
                CurrentFrame()->AdvanceProgramCounter();
                Integer arg1 = byteCode->SmallArgument1();
                Integer arg2 = byteCode->SmallArgument2();
                this->LoadGlobal(arg1, arg2);
                break;
            }
            case ByteCodeType::Return: {
                CurrentFrame()->AdvanceProgramCounter();
                Integer arg1 = byteCode->SmallArgument1();
                this->Return(arg1);
                return;
            }
            case ByteCodeType::Copy: {
                CurrentFrame()->AdvanceProgramCounter();
                Integer arg1 = byteCode->SmallArgument1();
                Integer arg2 = byteCode->SmallArgument2();
                this->Copy(arg1, arg2);
                break;
            }
            case ByteCodeType::Equal: {
                CurrentFrame()->AdvanceProgramCounter();
                Integer arg1 = byteCode->SmallArgument1();
                Integer arg2 = byteCode->SmallArgument2();
                Integer arg3 = byteCode->SmallArgument3();
                this->Equal(arg1, arg2, arg3);
                break;
            }
            case ByteCodeType::Subtract: {
                CurrentFrame()->AdvanceProgramCounter();
                Integer arg1 = byteCode->SmallArgument1();
                Integer arg2 = byteCode->SmallArgument2();
                Integer arg3 = byteCode->SmallArgument3();
                this->Subtract(arg1, arg2, arg3);
                break;
            }
            case ByteCodeType::Add: {
                CurrentFrame()->AdvanceProgramCounter();
                Integer arg1 = byteCode->SmallArgument1();
                Integer arg2 = byteCode->SmallArgument2();
                Integer arg3 = byteCode->SmallArgument3();
                this->Add(arg1, arg2, arg3);
                break;
            }
            case ByteCodeType::Multiply: {
                CurrentFrame()->AdvanceProgramCounter();
                Integer arg1 = byteCode->SmallArgument1();
                Integer arg2 = byteCode->SmallArgument2();
                Integer arg3 = byteCode->SmallArgument3();
                this->Multiply(arg1, arg2, arg3);
                break;
            }
            case ByteCodeType::JumpIfFalse: {
                CurrentFrame()->AdvanceProgramCounter();
                Integer arg1 = byteCode->SmallArgument1();
                Integer dest = byteCode->LargeArgument();
                bool result = Local(arg1)->IsTruthy();
                if (!result) {
                    CurrentFrame()->SetProgramCounter(dest);
                }
                break;
            }
            case ByteCodeType::Not: {
                CurrentFrame()->AdvanceProgramCounter();
                Integer dest = byteCode->SmallArgument1();
                Integer source = byteCode->SmallArgument2();
                this->Not(dest, source);
                break;
            }
            case ByteCodeType::Jump: {
                CurrentFrame()->SetProgramCounter(byteCode->LargeArgument());
                break;
            }
            case ByteCodeType::StoreGlobal: {
                CurrentFrame()->AdvanceProgramCounter();
                Integer key = byteCode->SmallArgument1();
                Integer value = byteCode->SmallArgument2();
                this->StoreGlobal(key, value);
                break;
            }
            default: {
                Panic("Unknown ByteCode in Interpret");
                break;
            }
        }
    }
}

Value* Runtime::Local(Integer num) {
    return CurrentFrame()->At(this, num);
}

Value* CallFrame::At(Runtime* rt, Integer index) {
    if (index.Unwrap() >= this->stackSize.Unwrap() || index.Unwrap() < 0) {
        Panic("Stack underflow");
        return nullptr;
    }

    std::int64_t absoluteIndex = this->stackBase.Unwrap() + index.Unwrap();

    return rt->StackAtAbsoluteIndex(Integer{absoluteIndex});
}

Value* Runtime::StackAtAbsoluteIndex(Integer index) {
    return this->stack.At(index);
}

void Runtime::Copy(Integer destIndex, Integer sourceIndex) {
    this->Local(destIndex)->Copy(this->Local(sourceIndex));
}

void Runtime::Equal(Integer dest, Integer arg1, Integer arg2) {
    Local(dest)->SetBoolean(Local(arg1)->Equals(this, Local(arg2)));
}

void Runtime::Not(Integer dest, Integer source) {
    Local(dest)->SetBoolean(!Local(source)->GetBoolean(this));
}

void Runtime::Add(Integer dest, Integer arg1, Integer arg2) {
    switch (Local(arg1)->GetType()) {
        case ValueType::Integer: {
            Local(dest)->SetInteger(
                Integer{
                    Local(arg1)->GetInteger(this).Unwrap() + Local(arg2)->GetInteger(this).Unwrap()
                }
            );
            break;
        }
        case ValueType::Double: {
            Local(dest)->SetDouble(
                Double{
                    Local(arg1)->GetDouble(this).Unwrap() + Local(arg2)->GetDouble(this).Unwrap()
                }
            );
            break;
        }
        default: {
            this->Local(Integer{0})->SetString(this->NewString("Expected an integer or real for math operation"));
            this->Throw(Integer{0});
            break;
        }
    }
}

void Runtime::Subtract(Integer dest, Integer arg1, Integer arg2) {
    switch (Local(arg1)->GetType()) {
        case ValueType::Integer: {
            Local(dest)->SetInteger(
                Integer{
                    Local(arg1)->GetInteger(this).Unwrap() - Local(arg2)->GetInteger(this).Unwrap()
                }
            );
            break;
        }
        case ValueType::Double: {
            Local(dest)->SetDouble(
                Double{
                    Local(arg1)->GetDouble(this).Unwrap() - Local(arg2)->GetDouble(this).Unwrap()
                }
            );
            break;
        }
        default: {
            this->Local(Integer{0})->SetString(this->NewString("Expected an integer or real for math operation"));
            this->Throw(Integer{0});
            break;
        }
    }
}

void Runtime::Multiply(Integer dest, Integer arg1, Integer arg2) {
    switch (Local(arg1)->GetType()) {
        case ValueType::Integer: {
            Local(dest)->SetInteger(
                Integer{
                    Local(arg1)->GetInteger(this).Unwrap() * Local(arg2)->GetInteger(this).Unwrap()
                }
            );
            break;
        }
        case ValueType::Double: {
            Local(dest)->SetDouble(
                Double{
                    Local(arg1)->GetDouble(this).Unwrap() * Local(arg2)->GetDouble(this).Unwrap()
                }
            );
            break;
        }
        default: {
            this->Local(Integer{0})->SetString(this->NewString("Expected an integer or real for math operation"));
            this->Throw(Integer{0});
            break;
        }
    }
}

ValueType Value::GetType() const {
    return this->type;
}

void Runtime::Throw(Integer idx) {
    Integer stackIndex = Integer{CurrentFrame()->AbsoluteIndex(idx)};
    throw ThrowException{stackIndex};
}

NativeFunction* Runtime::NewNativeFunction(Integer arity, Integer localCount, NativeFunction::Handle fn) {
    NativeFunction* function = this->New<NativeFunction>(Integer{1});
    function->Init(this, this->heap, arity, localCount, fn);
    this->heap = function;
    return function;
}

void NativeFunction::Init(Runtime* rt, Object* next, Integer arity, Integer locals, NativeFunction::Handle fn) {
    (void)(rt);

    this->ObjectInit(ObjectType::NativeFunction, next);
    this->arity = arity;
    this->localCount = locals;
    this->handle = fn;
}

Integer NativeFunction::GetArity() const {
    return this->arity;
}

Integer NativeFunction::GetLocalCount() const {
    return this->localCount;
}

NativeFunction::Handle NativeFunction::GetHandle() const {
    return this->handle;
}

void Runtime::DefineGlobal(Integer keyIndex, Integer valueIndex) {
    Value* key = Local(keyIndex);
    Value* value = Local(valueIndex);
    globals->Put(this, key, value);
}

String* Runtime::NewString(const char* message) {
    // TODO: size converstion checking
    Integer length = Integer{static_cast<std::int64_t>(std::strlen(message))};
    String* str = this->New<String>(Integer{1});
    str->Init(this, this->heap, length, message);
    this->heap = str;
    return str;
}

void String::Init(Runtime* rt, Object* next, Integer length, const char* data) {
    this->ObjectInit(ObjectType::String, next);
    this->data.InitWithCapacity(rt, Integer{1 + length.Unwrap()});
    std::int64_t n = length.Unwrap();
    for (std::int64_t i = 0; i < n; i++) {
        *this->data.Push(rt) = data[i];
    }
    *this->data.Push(rt) = '\0';
}

void Object::ObjectInit(ObjectType type, Object* next) {
    this->type = type;
    this->next = next;
}

Function* Runtime::NewFunction() {
    Function* function = this->New<Function>(Integer{1});
    function->Init(this, this->heap);
    this->heap = function;
    return function;
}

Map* Runtime::NewMap() {
    Map* map = this->New<Map>(Integer{1});
    map->Init(this, this->heap);
    this->heap = map;
    return map;
}

void Function::Init(Runtime* rt, Object* next) {
    this->ObjectInit(ObjectType::Function, next);
    this->arity = Integer{0};
    this->byteCode.Init(rt);
    this->constants.Init(rt);
}

Integer Function::GetLocalCount() const {
    return this->localCount;
}

Integer Function::GetConstantCount() const {
    return this->constants.Length();
}

Integer Function::GetByteCodeCount() const {
    return this->byteCode.Length();
}

ByteCodeType ByteCode::Type() const {
    return static_cast<ByteCodeType>(this->value & bits::OP_BITS);
}

Integer ByteCode::SmallArgument1() const {
    return Integer{(this->value & bits::ARG1_BITS) >> bits::ARG1_SHIFT};
}

Integer ByteCode::SmallArgument2() const {
    return Integer{(this->value & bits::ARG2_BITS) >> bits::ARG2_SHIFT};
}

Integer ByteCode::SmallArgument3() const {
    return Integer{(this->value & bits::ARG3_BITS) >> bits::ARG3_SHIFT};
}

Integer ByteCode::LargeArgument() const {
    return Integer{(this->value & bits::LARGE_ARG_BITS) >> bits::LARGE_ARG_SHIFT};
}

ByteCode* Function::ByteCodeAt(Integer index) const {
    return this->byteCode.At(index);
}

Value* Function::ConstantAt(Integer index) const {
    return this->constants.At(index);
}

Integer Function::GetArity() const {
    return this->arity;
}

bool Value::IsTruthy() const {
    switch (this->GetType()) {
        case ValueType::Boolean: {
            return static_cast<bool>(this->as.integer.Unwrap());
        }
        case ValueType::Nil: {
            return false;
        }
        default: {
            return true;
        }
    }
}

void Value::Copy(Value* other) {
    this->type = other->type;
    this->as = other->as;
}

void Value::SetDouble(Double val) {
    this->as.real = val;
    this->type = ValueType::Double;
}

Double Value::GetDouble(Runtime* rt) const {
    this->AssertType(rt, ValueType::Double);
    return this->as.real;
}

void Value::SetInteger(Integer val) {
    this->as.integer = val;
    this->type = ValueType::Integer;
}

Integer Value::GetInteger(Runtime* rt) const {
    this->AssertType(rt, ValueType::Integer);
    return this->as.integer;
}

void Value::SetBoolean(bool val) {
    this->as.integer = Integer{val};
    this->type = ValueType::Boolean;
}

bool Value::GetBoolean(Runtime* rt) const {
    this->AssertType(rt, ValueType::Boolean);
    return static_cast<bool>(this->as.integer.Unwrap());
}

void Value::SetNativeFunction(NativeFunction* val) {
    this->as.nativeFunction = val;
    this->type = ValueType::NativeFunction;
}

NativeFunction* Value::GetNativeFunction(Runtime* rt) const {
    this->AssertType(rt, ValueType::NativeFunction);
    return this->as.nativeFunction;
}

void Value::SetFunction(Function* val) {
    this->as.function = val;
    this->type = ValueType::Function;
}

Function* Value::GetFunction(Runtime* rt) const {
    this->AssertType(rt, ValueType::Function);
    return this->as.function;
}

void Value::SetString(String* val) {
    this->as.string = val;
    this->type = ValueType::String;
}

String* Value::GetString(Runtime* rt) const {
    this->AssertType(rt, ValueType::String);
    return this->as.string;
}

void Value::SetMap(Map* val) {
    this->as.map = val;
    this->type = ValueType::Map;
}

Map* Value::GetMap(Runtime* rt) const {
    this->AssertType(rt, ValueType::Map);
    return this->as.map;
}

void Value::AssertType(Runtime* rt, ValueType expected) const {
    if (this->GetType() == expected) {
        return;
    }
    rt->Local(Integer{0})->SetString(rt->NewString("Illegal Cast"));
    rt->Throw(Integer{0});
}

void CallFrame::AdvanceProgramCounter() {
    this->programCounter = Integer{this->programCounter.Unwrap() + 1};
}

void CallFrame::SetProgramCounter(Integer newPc) {
    this->programCounter = newPc;
}

Integer CallFrame::ProgramCounter() const {
    return this->programCounter;
}

void Runtime::StoreGlobal(Integer keyIndex, Integer valueIndex) {
    Value* key = Local(keyIndex);
    Value* value = Local(valueIndex);

    key->AssertType(this, ValueType::String);

    this->globals->Put(this, key, value);
}

void Runtime::LoadGlobal(Integer destIndex, Integer keyIndex) {
    Value* dest = Local(destIndex);
    Value* key = Local(keyIndex);

    key->AssertType(this, ValueType::String);

    Value* result = this->globals->Get(this, key);

    if (result != nullptr) {
        dest->Copy(result);
        return;
    } else {
        dest->SetNil();
    }

    this->Local(Integer{0})->SetString(this->NewString("Undefined Global"));
    this->Throw(Integer{0});
}

Value* Map::Get(Runtime* rt, Value* key) {
    std::int64_t n = this->entries.Length().Unwrap();
    for (std::int64_t index = 0; index < n; index++) {
        Entry* entry = this->entries.At(Integer{index});
        if (entry->key.Equals(rt, key)) {
            return &entry->value;
        }
    }
    return nullptr;
}

void Map::Put(Runtime* rt, Value* key, Value* value) {
    Entry* existing = nullptr;
    std::int64_t n = this->entries.Length().Unwrap();
    for (std::int64_t index = 0; index < n; index++) {
        Entry* entry = this->entries.At(Integer{index});
        if (entry->key.Equals(rt, key)) {
            existing = entry;
            break;
        }
    }
    if (existing == nullptr) {
        existing = this->entries.Push(rt);
    }
    existing->key.Copy(key);
    existing->value.Copy(value);
}

void Map::Init(Runtime* rt, Object* next) {
    this->ObjectInit(ObjectType::Map, next);
    this->entries.Init(rt);
}

bool Value::Equals(Runtime* rt, Value* other) const {
    if (this->GetType() != other->GetType()) {
        return false;
    }
    switch (this->GetType()) {
        case ValueType::Boolean: {
            return this->GetBoolean(rt) == other->GetBoolean(rt);
        }
        case ValueType::Double: {
            return this->GetDouble(rt).Unwrap() == other->GetDouble(rt).Unwrap();
        }
        case ValueType::Function: {
            return this->GetFunction(rt) == other->GetFunction(rt);
        }
        case ValueType::Integer: {
            return this->GetInteger(rt).Unwrap() == other->GetInteger(rt).Unwrap();
        }
        case ValueType::NativeFunction: {
            return this->GetNativeFunction(rt) == other->GetNativeFunction(rt);
        }
        case ValueType::Nil: {
            return true;
        }
        case ValueType::String: {
            return this->GetString(rt)->Equals(other->GetString(rt));
        }
        case ValueType::Map: {
            // TODO: consider making this equal by value rather than reference?
            return this->GetMap(rt) == other->GetMap(rt);
        }
        default: {
            Panic("Unhandled ValueType in Equals");
            return false;
        }
    }
}

bool String::Equals(String* other) const {
    std::int64_t n = this->data.Length().Unwrap();
    if (other->data.Length().Unwrap() != n) {
        return false;
    }
    for (std::int64_t i = 0; i < n; i++) {
        Integer wrapped = Integer{i};
        if (*other->data.At(wrapped) != *this->data.At(wrapped)) {
            return false;
        }
    }
    return true;
}

void String::Push(Runtime* rt, char c) {
    *this->data.Push(rt) = c;
}

void String::Clear() {
    this->data.Truncate(Integer{0});
}

Integer String::Length() const {
    return this->data.Length();
}

void String::Reserve(Runtime* rt, Integer cap) {
    this->data.Reserve(rt, cap);
}

char String::At(Integer idx) const {
    return *this->data.At(idx);
}

void Function::ReserveByteCode(Runtime* rt, Integer val) {
    this->byteCode.Reserve(rt, val);
}

ByteCode* Function::PushByteCode(Runtime* rt) {
    return this->byteCode.Push(rt);
}

void Function::ReserveConstants(Runtime* rt, Integer val) {
    this->constants.Reserve(rt, val);
}

Value* Function::PushConstant(Runtime* rt) {
    return this->constants.Push(rt);
}

void Function::SetStack(Integer arity, Integer localCount) {
    this->arity = arity;
    this->localCount = localCount;
}

void ByteCode::Init(Runtime* rt, uint32_t val) {
    (void)(rt);

    this->value = val;
}


const char* String::RawPointer() const {
    return this->data.RawHeadPointer();
}

void Function::Verify(Runtime* rt) const {
    if (this->arity.Unwrap() > this->localCount.Unwrap()) {
        rt->Local(Integer{0})->SetString(rt->NewString("Invalid arity for function. Must be <= localCount"));
        rt->Throw(Integer{0});
    }

    if (this->arity.Unwrap() <= 0) {
        rt->Local(Integer{0})->SetString(rt->NewString("Invalid arity for function. Must be >= 1"));
        rt->Throw(Integer{0});
    }

    if (this->localCount.Unwrap() <= 0) {
        rt->Local(Integer{0})->SetString(rt->NewString("Invalid localCount for function. Must be >= 1"));
        rt->Throw(Integer{0});
    }

    std::int64_t byteCodeCount = this->byteCode.Length().Unwrap();
    for (std::int64_t i = 0; i < byteCodeCount; i++) {
        ByteCode* bc = this->ByteCodeAt(Integer{i});
        bc->Verify(rt, this);
    }

    std::int64_t constantCount = this->constants.Length().Unwrap();
    for (std::int64_t i = 0; i < constantCount; i++) {
        Value* value = this->ConstantAt(Integer{i});
        switch (value->GetType()) {
            case ValueType::Function: {
                value->GetFunction(rt)->Verify(rt);
                break;
            }
            case ValueType::NativeFunction: {
                value->GetNativeFunction(rt)->Verify(rt);
                break;
            }
            default: {
                break;
            }
        }
    }
}

void ByteCode::Verify(Runtime* rt, const Function* fn) const {


    std::int64_t constantCount = fn->GetConstantCount().Unwrap();
    std::int64_t localCount = fn->GetLocalCount().Unwrap();
    std::int64_t byteCodeCount = fn->GetByteCodeCount().Unwrap();

    auto validateProgramCounter = [&](Integer val, const char* message) {
        std::int64_t newPc = val.Unwrap();
        if (newPc < 0 || newPc >= byteCodeCount) {
            rt->Local(Integer{0})->SetString(rt->NewString(message));
            rt->Throw(Integer{0});
        }
    };

    auto validateRegisterIsReadable = [&](Integer registerArg, const char* message) {
        std::int64_t regArg = registerArg.Unwrap();
        if (regArg >= localCount || regArg < 0) {
            rt->Local(Integer{0})->SetString(rt->NewString(message));
            rt->Throw(Integer{0});
        }
    };

    auto validateRegisterIsWritable = [&](Integer registerArg, const char* message) {
        std::int64_t regArg = registerArg.Unwrap();
        if (regArg >= localCount || regArg <= 0) {
            rt->Local(Integer{0})->SetString(rt->NewString(message));
            rt->Throw(Integer{0});
        }
    };

    auto validateConstantIsReadable = [&](Integer constantArg, const char* message) {
        std::int64_t constArg = constantArg.Unwrap();
        if (constArg >= constantCount) {
            rt->Local(Integer{0})->SetString(rt->NewString(message));
            rt->Throw(Integer{0});
        }
    };

    // auto validateConstantIsType = [&](Integer constantArg, ValueType expected, const char* message) {
    //     ValueType actualType = fn->ConstantAt(constantArg)->GetType();
    //     if (actualType != expected) {
    //         rt->Local(Integer{0})->SetString(rt->NewString(message));
    //         rt->Throw(Integer{0});
    //     }
    // };

    switch (this->Type()) {
        case ByteCodeType::NoOp: {
            break;
        }
        case ByteCodeType::Return: {
            validateRegisterIsReadable(this->SmallArgument1(), "Invalid readable register for Return instruction");
            break;
        }
        case ByteCodeType::LoadConstant: {
            validateRegisterIsWritable(this->SmallArgument1(), "Invalid writable register for LoadConstant");
            validateConstantIsReadable(this->LargeArgument(), "Invalid constant for LoadConstant");
            break;
        }
        case ByteCodeType::LoadGlobal: {
            validateRegisterIsWritable(this->SmallArgument1(), "Invalid writable register for LoadGlobal");
            validateRegisterIsReadable(this->SmallArgument2(), "Invalid readable register for LoadGlobal");
            break;
        }
        case ByteCodeType::Invoke: {
            validateRegisterIsWritable(this->SmallArgument1(), "Invalid writable register for Invoke");
            std::int64_t argCount = this->SmallArgument2().Unwrap();
            if (argCount <= 0) {
                rt->Local(Integer{0})->SetString(rt->NewString("Invalid argumentCount in Invoke"));
                rt->Throw(Integer{0});
            }
            break;
        }
        case ByteCodeType::Copy: {
            validateRegisterIsWritable(this->SmallArgument1(), "Invalid writable register for Copy");
            validateRegisterIsReadable(this->SmallArgument2(), "Invalid readable register for Copy");
            break;
        }
        case ByteCodeType::Equal: {
            validateRegisterIsWritable(this->SmallArgument1(), "Invalid writable register for Equal");
            validateRegisterIsReadable(this->SmallArgument2(), "Invalid readable register 1 for Equal");
            validateRegisterIsReadable(this->SmallArgument3(), "Invalid readable register 2 for Equal");
            break;
        }
        case ByteCodeType::Add: {
            validateRegisterIsWritable(this->SmallArgument1(), "Invalid writable register for Add");
            validateRegisterIsReadable(this->SmallArgument2(), "Invalid readable register 1 for Add");
            validateRegisterIsReadable(this->SmallArgument3(), "Invalid readable register 2 for Add");
            break;
        }
        case ByteCodeType::Subtract: {
            validateRegisterIsWritable(this->SmallArgument1(), "Invalid writable register for Subtract");
            validateRegisterIsReadable(this->SmallArgument2(), "Invalid readable register 1 for Subtract");
            validateRegisterIsReadable(this->SmallArgument3(), "Invalid readable register 2 for Subtract");
            break;
        }
        case ByteCodeType::Multiply: {
            validateRegisterIsWritable(this->SmallArgument1(), "Invalid writable register for Multiply");
            validateRegisterIsReadable(this->SmallArgument2(), "Invalid readable register 1 for Multiply");
            validateRegisterIsReadable(this->SmallArgument3(), "Invalid readable register 2 for Multiply");
            break;
        }
        case ByteCodeType::JumpIfFalse: {
            validateRegisterIsReadable(this->SmallArgument1(), "Invalid readable register for JumpIfFalse");
            validateProgramCounter(this->LargeArgument(), "Invalid program counter for JumpIfFalse");
            break;
        }
        case ByteCodeType::Not: {
            validateRegisterIsWritable(this->SmallArgument1(), "Invalid writable register for Not");
            validateRegisterIsReadable(this->SmallArgument2(), "Invalid readable register for Not");
            break;
        }
        case ByteCodeType::Jump: {
            validateProgramCounter(this->LargeArgument(), "Invalid program counter for Jump");
            break;
        }
        case ByteCodeType::StoreGlobal: {
            validateRegisterIsWritable(this->SmallArgument1(), "Invalid readable register 1 for StoreGlobal");
            validateRegisterIsReadable(this->SmallArgument2(), "Invalid readable register 2 for StoreGlobal");
            break;
        }
        default: {
            Panic("Unhandled bytecode in bytecode verifier");
            return;
        }
    }
}

void NativeFunction::Verify(Runtime* rt) const {
    if (this->arity.Unwrap() > this->localCount.Unwrap()) {
        rt->Local(Integer{0})->SetString(rt->NewString("Invalid arity for nativefunction. Must be <= localCount"));
        rt->Throw(Integer{0});
    }

    if (this->arity.Unwrap() <= 0) {
        rt->Local(Integer{0})->SetString(rt->NewString("Invalid arity for nativefunction. Must be >= 1"));
        rt->Throw(Integer{0});
    }

    if (this->localCount.Unwrap() <= 0) {
        rt->Local(Integer{0})->SetString(rt->NewString("Invalid localCount for nativefunction. Must be >= 1"));
        rt->Throw(Integer{0});
    }
}


}
