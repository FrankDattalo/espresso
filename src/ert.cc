#include "ert.hh"
#include "esys.hh"

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

    // setup empty value so that 
    // call frames are in proper state
    this->stack.Push(this)->SetNil();

    CallFrame* initialFrame = this->frames.Push(this);
    initialFrame->Init(Integer{0}, Integer{0});

    this->PushMap();
    this->globals = Top()->GetMap(this);
    this->Pop();

    // TODO: make this parameterizeable?
    // TODO: schma for file formatting
    this->PushString("lib/entry.bc");
    this->ReadFile();
    this->LoadByteCode();
    // remove the temp nil
    this->Swap();
    this->Pop();

    struct NativeEntry {
        const char* name;
        NativeFunction fn;
    };

    constexpr static NativeEntry NATIVES[] = {
        { "print", Runtime::DoPrint },
    };

    for (const auto& native: NATIVES) {
        PushString(native.name);
        PushNativeFunction(native.fn);
        DefineGlobal();
    }
}

void Runtime::DeInit() {
    this->stack.DeInit(this);
    this->frames.DeInit(this);
}

System* Runtime::GetSystem() {
    return this->system;
}

void Runtime::Invoke(Integer argumentCount) {
    // -1 for function itself
    Integer stackBase = Integer{
        Frame()->AbsoluteStackSize().Unwrap() - argumentCount.Unwrap() - 1};
    ValueType val = Frame()->At(this, stackBase)->GetType();

    if (!(val == ValueType::Function || val == ValueType::NativeFunction)) {
        Panic("Illegal Cast to Function");
        return;
    }

    CallFrame* frame = frames.Push(this);
    frame->Init(stackBase, argumentCount);

    if (val == ValueType::Function) {
        Integer arity = Frame()->At(this, stackBase)->GetFunction(this)->GetArity();
        if (argumentCount.Unwrap() != arity.Unwrap()) {
            this->PushString("Invalid arity");
            this->Throw();
            return;
        }
        this->Interpret();

    } else /* val == ValueType::NativeFunction */ {
        NativeFunction fn = frame->At(this, Integer{0})->GetNativeFunction(this);
        fn(this);
    }

    // return logic of transfering stack result to the right spot
    Integer resultFrameIndex = Integer{this->frames.Length().Unwrap() - 2};
    CallFrame* resultFrame = this->frames.At(resultFrameIndex);
    Integer currentResultFrameSize = resultFrame->Size();
    // -1 for function itself should be here, but this is negated by the push of the result
    Integer newResultFrameSize = Integer{currentResultFrameSize.Unwrap() - argumentCount.Unwrap()};
    resultFrame->SetSize(newResultFrameSize);
    resultFrame->At(this, Integer{newResultFrameSize.Unwrap() - 1})->Copy(this, Top());
    this->frames.Pop();
    this->stack.Truncate(resultFrame->AbsoluteStackSize());
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

CallFrame* Runtime::Frame() {
    Integer length = frames.Length();
    Integer last = Integer{length.Unwrap() - 1};
    return frames.At(last);
}

void CallFrame::Init(Integer stackBase, Integer argumentCount) {
    this->stackBase = stackBase;
    this->programCounter = Integer{0};
    // 1 for the function itself
    this->stackSize = Integer{1 + argumentCount.Unwrap()};
}

Integer CallFrame::AbsoluteStackSize() const {
    return Integer{this->stackBase.Unwrap() + this->stackSize.Unwrap()};
}

void Value::SetNil() {
    this->type = ValueType::Nil;
    this->as.integer = Integer{0};
}

void Runtime::Interpret() {
    while (true) {
        Function* function = Frame()->At(this, Integer{0})->GetFunction(this);
        ByteCode* byteCode = function->ByteCodeAt(Frame()->ProgramCounter());
        switch (byteCode->Type()) {
            case ByteCodeType::NoOp: {
                Frame()->AdvanceProgramCounter();
                break;
            }
            case ByteCodeType::Invoke: {
                Frame()->AdvanceProgramCounter();
                Invoke(byteCode->Argument());
                break;
            }
            case ByteCodeType::LoadConstant: {
                Frame()->AdvanceProgramCounter();
                this->PushValue(function->ConstantAt(byteCode->Argument()));
                break;
            }
            case ByteCodeType::LoadGlobal: {
                Frame()->AdvanceProgramCounter();
                this->LoadGlobal();
                break;
            }
            case ByteCodeType::Pop: {
                Frame()->AdvanceProgramCounter();
                this->Pop();
                break;
            }
            case ByteCodeType::Return: {
                return;
            }
            default: {
                Panic("Unknown ByteCode");
                break;
            }
        }
    }
}

Value* CallFrame::At(Runtime* rt, Integer index) {
    if (index.Unwrap() >= this->stackSize.Unwrap() || index.Unwrap() < 0) {
        Panic("Stack underflow");
        return nullptr;
    }

    std::int64_t absoluteIndex = this->stackBase.Unwrap() + index.Unwrap();

    return rt->StackAt(Integer{absoluteIndex});
}

Value* Runtime::StackAt(Integer index) {
    return this->stack.At(index);
}

ValueType Value::GetType() const {
    return this->type;
}

void Runtime::Throw() {
    Integer stackIndex = Integer{Frame()->AbsoluteStackSize().Unwrap() - 1};
    throw ThrowException{stackIndex};
}

void Runtime::PushNativeFunction(NativeFunction fn) {
    PushNil();
    Top()->SetNativeFunction(fn);
}

void Runtime::DefineGlobal() {
    CallFrame* frame = Frame();
    Integer value = Integer{frame->Size().Unwrap() - 1};
    Integer key = Integer{frame->Size().Unwrap() - 2};
    globals->Put(this, frame->At(this, key), frame->At(this, value));
    Pop();
    Pop();
}

void Runtime::PushString(const char* message) {
    PushNil();
    // TODO: size converstion checking
    Integer length = Integer{static_cast<std::int64_t>(std::strlen(message))};
    String* str = this->New<String>(Integer{1});
    str->Init(this, this->heap, length, message);
    Top()->SetString(str);
    this->heap = str;
}

Value* Runtime::Top() {
    CallFrame* frame = Frame();
    return frame->At(this, Integer{frame->Size().Unwrap() - 1});
}

Integer CallFrame::Size() const {
    return this->stackSize;
}

void CallFrame::SetSize(Integer value) {
    if (value.Unwrap() < 0) {
        Panic("SetSize");
        return;
    }
    this->stackSize = value;
}

void Runtime::PushNil() {
    CallFrame* frame = Frame();
    Integer prevSize = frame->Size();
    this->stack.Push(this)->SetNil();
    frame->SetSize(Integer{1 + prevSize.Unwrap()});
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

void Runtime::PushFunction() {
    PushNil();
    Function* function = this->New<Function>(Integer{1});
    function->Init(this, this->heap);
    Top()->SetFunction(function);
    this->heap = function;
}

void Runtime::PushMap() {
    PushNil();
    Map* map = this->New<Map>(Integer{1});
    map->Init(this, this->heap);
    Top()->SetMap(map);
    this->heap = map;
}

void Function::Init(Runtime* rt, Object* next) {
    this->ObjectInit(ObjectType::Function, next);
    this->arity = Integer{0};
    this->byteCode.Init(rt);
    this->constants.Init(rt);
}

ByteCodeType ByteCode::Type() const {
    return this->type;
}

Integer ByteCode::Argument() const {
    return this->argument;
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

void Value::SetNativeFunction(NativeFunction val) {
    this->as.native = val;
    this->type = ValueType::NativeFunction;
}

NativeFunction Value::GetNativeFunction(Runtime* rt) const {
    this->AssertType(rt, ValueType::NativeFunction);
    return this->as.native;
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
    rt->PushString("Illegal Cast");
    rt->Throw();
}

void CallFrame::AdvanceProgramCounter() {
    this->programCounter = Integer{this->programCounter.Unwrap() + 1};
}

Integer CallFrame::ProgramCounter() const {
    return this->programCounter;
}

void Runtime::Pop() {
    CallFrame* frame = Frame();
    Integer currentSize = frame->Size();
    frame->SetSize(Integer{currentSize.Unwrap() - 1});
    this->stack.Truncate(frame->AbsoluteStackSize());
}

void Runtime::PushValue(Value* other) {
    PushNil();
    Top()->Copy(this, other);
}

void Runtime::LoadGlobal() {
    Value* key = Top();
    key->AssertType(this, ValueType::String);
    Value* result = this->globals->Get(this, key);
    if (result != nullptr) {
        key->Copy(this, result);
        return;
    }
    PushString("Undefined");
    Throw();
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

void Function::SetArity(Integer arity) {
    this->arity = arity;
}

void Runtime::Swap() {
    PushNil();
    CallFrame* frame = Frame();
    std::int64_t temp = frame->Size().Unwrap() - 1;
    std::int64_t top = frame->Size().Unwrap() - 2;
    std::int64_t bottom = frame->Size().Unwrap() - 3;
    frame->At(this, Integer{temp})->Copy(this, frame->At(this, Integer{bottom}));
    frame->At(this, Integer{bottom})->Copy(this, frame->At(this, Integer{top}));
    frame->At(this, Integer{top})->Copy(this, frame->At(this, Integer{temp}));
    Pop();
}

void ByteCode::Init(Runtime* rt, uint16_t val) {
    uint16_t op = val & OP_BITS;
    uint16_t value = val & VALUE_BITS;
    switch (op) {
        case OP_LOAD_CONSTANT: {
            this->type = ByteCodeType::LoadConstant;
            this->argument = Integer{value};
            return;
        }
        case OP_LOAD_GLOBAL: {
            this->type = ByteCodeType::LoadGlobal;
            this->argument = Integer{value};
            return;
        }
        case OP_INVOKE: {
            this->type = ByteCodeType::Invoke;
            this->argument = Integer{value};
            return;
        }
        case OP_POP: {
            this->type = ByteCodeType::Pop;
            this->argument = Integer{value};
            return;
        }
        case OP_RETURN: {
            this->type = ByteCodeType::Return;
            this->argument = Integer{value};
            return;
        }
        default: {
            rt->PushString("Invalid ByteCode");
            rt->Throw();
            return;
        }
    }
}

void Runtime::ReadFile() {
    String* fileName = Top()->GetString(this);
    const char* fileNameCString = fileName->RawPointer();
    FILE* fp = system->Open(fileNameCString, "rb");
    if (fp == nullptr) {
        PushString("Could not open file");
        Throw();
        return;
    }
    PushString("");
    String* dest = Top()->GetString(this);
    int c = EOF;
    dest->Clear();
    while ((c = system->Read(fp)) != EOF) {
        dest->Push(this, static_cast<char>(c));
    }
    system->Close(fp);
    dest->Push(this, '\0');
    Swap();
    Pop();
}

const char* String::RawPointer() const {
    return this->data.RawHeadPointer();
}

void Runtime::DoPrint(Runtime* rt) {
    String* str = rt->Top()->GetString(rt);
    rt->system->Write(rt->system->Stdout(), str->RawPointer(), str->Length().Unwrap());
    rt->system->Write(rt->system->Stdout(), "\n", 1);
    rt->PushNil();
}

}
