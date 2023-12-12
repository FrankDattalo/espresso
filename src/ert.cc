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
    initialFrame->Init(Integer{0}, Integer{2});
    // null out the initial stack
    this->stack.Push(this)->SetNil();
    this->stack.Push(this)->SetNil();

    // load natives
    Local(Integer{0})->SetNativeFunction(NewNativeFunction(Integer{1}, Integer{2}, espresso::native::RegisterNatives));
    Invoke(Integer{0}, Integer{1});

    // load entrypoint
    Local(Integer{0})->SetNativeFunction(NewNativeFunction(Integer{1}, Integer{2}, espresso::native::Entrypoint));
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

    if (localCount.Unwrap() <= 0) {
        Panic("Invalid local count");
        return;
    }

    if (argumentCount.Unwrap() <= 0) {
        Panic("Invalid argument count");
        return;
    }

    if (argumentCount.Unwrap() > localCount.Unwrap()) {
        Panic("Invalid argument count");
        return;
    }

    if (arity.Unwrap() != argumentCount.Unwrap()) {
        Local(Integer{0})->SetString(NewString("Invalid arity"));
        Throw(Integer{0});
        return;
    }

    Integer absoluteBase = CurrentFrame()->AbsoluteIndex(localBase);
    frames.Push(this)->Init(absoluteBase, localCount);

    // prepare the new stack

    // 1. all the locals that are consumed by arguments should be set to nil
    // 2. the actual stack needs to expand to fit all the new locals
    std::int64_t newStackTop = CurrentFrame()->AbsoluteIndex(localCount).Unwrap();
    // +1 for function itself
    std::int64_t lastValidIndex = CurrentFrame()->AbsoluteIndex(argumentCount).Unwrap();
    for(std::int64_t i = CurrentFrame()->AbsoluteIndex(Integer{0}).Unwrap(); i < newStackTop; i++) {
        if (stack.Length().Unwrap() <= i) {
            stack.Push(this)->SetNil();
        }
        if (i > lastValidIndex) {
            Local(Integer{i})->SetNil();
        }
    }

    // enter the actual function here

    if (fnType == ValueType::Function) {
        this->Interpret();

    } else /* val == ValueType::NativeFunction */ {
        NativeFunction* fn = Local(Integer{0})->GetNativeFunction(this);
        NativeFunction::Handle handle = fn->GetHandle();
        handle(this);
    }

    // pop from the frame, return is already in the right spot
    this->frames.Pop();
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

void Value::SetNil() {
    this->type = ValueType::Nil;
    this->as.integer = Integer{0};
}

void Runtime::LoadConstant(Integer dest, Integer constant) {
    Function* function = Local(Integer{0})->GetFunction(this);
    this->Local(dest)->Copy(this, function->ConstantAt(constant));
}

void Runtime::Return(Integer sourceIndex) {
    this->Local(Integer{0})->Copy(this, this->Local(sourceIndex));
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
                Invoke(byteCode->Argument1(), byteCode->Argument2());
                break;
            }
            case ByteCodeType::LoadConstant: {
                CurrentFrame()->AdvanceProgramCounter();
                LoadConstant(byteCode->Argument1(), byteCode->Argument2());
                break;
            }
            case ByteCodeType::LoadGlobal: {
                CurrentFrame()->AdvanceProgramCounter();
                this->LoadGlobal(byteCode->Argument1(), byteCode->Argument2());
                break;
            }
            case ByteCodeType::Return: {
                CurrentFrame()->AdvanceProgramCounter();
                this->Return(byteCode->Argument1());
                return;
            }
            default: {
                Panic("Unknown ByteCode");
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
    this->Local(destIndex)->Copy(this, this->Local(sourceIndex));
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

ByteCodeType ByteCode::Type() const {
    return this->type;
}

Integer ByteCode::Argument1() const {
    return this->argument1;
}

Integer ByteCode::Argument2() const {
    return this->argument2;
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

void Value::Copy(Runtime* rt, Value* other) {
    switch (other->GetType()) {
        case ValueType::Boolean: {
            this->SetBoolean(other->GetBoolean(rt));
            break;
        }
        case ValueType::Double: {
            this->SetDouble(other->GetDouble(rt));
            break;
        }
        case ValueType::Function: {
            this->SetFunction(other->GetFunction(rt));
            break;
        }
        case ValueType::Integer: {
            this->SetInteger(other->GetInteger(rt));
            break;
        }
        case ValueType::NativeFunction: {
            this->SetNativeFunction(other->GetNativeFunction(rt));
            break;
        }
        case ValueType::Nil: {
            this->SetNil();
            break;
        }
        case ValueType::String: {
            this->SetString(other->GetString(rt));
            break;
        }
        case ValueType::Map: {
            this->SetMap(other->GetMap(rt));
            break;
        }
        default: {
            Panic("Unhandled ValueType");
            return;
        }
    }
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

Integer CallFrame::ProgramCounter() const {
    return this->programCounter;
}

void Runtime::LoadGlobal(Integer destIndex, Integer keyIndex) {
    Value* dest = Local(destIndex);
    Value* key = Local(keyIndex);

    key->AssertType(this, ValueType::String);

    Value* result = this->globals->Get(this, key);

    if (result != nullptr) {
        dest->Copy(this, result);
        return;
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
    existing->key.Copy(rt, key);
    existing->value.Copy(rt, value);
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
            Panic("Unhandled ValueType");
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
    uint32_t op = val & OP_BITS;
    uint16_t arg1 = (val & ARG1_BITS) >> ARG_BITS_COUNT;
    uint16_t arg2 = val & ARG2_BITS;

    this->argument1 = Integer{arg1};
    this->argument2 = Integer{arg2};

    switch (op) {
        case OP_LOAD_CONSTANT: {
            this->type = ByteCodeType::LoadConstant;
            return;
        }
        case OP_LOAD_GLOBAL: {
            this->type = ByteCodeType::LoadGlobal;
            return;
        }
        case OP_INVOKE: {
            this->type = ByteCodeType::Invoke;
            return;
        }
        case OP_RETURN: {
            this->type = ByteCodeType::Return;
            return;
        }
        default: {
            rt->Local(Integer{0})->SetString(rt->NewString("Invalid ByteCode"));
            rt->Throw(Integer{0});
            return;
        }
    }
}


const char* String::RawPointer() const {
    return this->data.RawHeadPointer();
}


}
