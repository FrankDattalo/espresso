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

void Runtime::Init(System* system) {
    this->system = system;
    this->stack.Init(this);
    this->frames.Init(this);
    CallFrame* initialFrame = this->frames.Push(this);
    initialFrame->Init(Integer{0});
    // TODO: push entrypoint
    this->PushFunction();
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
    Integer stackBase = Integer{Frame()->StackTop().Unwrap() - argumentCount.Unwrap() - 1};
    ValueType val = Frame()->At(this, stackBase)->GetType();

    if (!(val == ValueType::Function || val == ValueType::NativeFunction)) {
        Panic("Illegal Cast to Function");
        return;
    }

    CallFrame* frame = frames.Push(this);
    frame->Init(stackBase);

    if (val == ValueType::Function) {
        this->Interpret();

    } else /* val == ValueType::NativeFunction */ {
        NativeFunction fn = frame->At(this, Integer{0})->GetNativeFunction(this);
        fn(this);
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

template<typename T>
void Vector<T>::Init(Runtime* rt) {
    this->size = Integer{0};
    this->capacity = Integer{16};
    this->data = rt->New<T>(this->capacity);
}

template<typename T>
void Vector<T>::DeInit(Runtime* rt) {
    rt->Free<T>(this->data, this->capacity);
}

template<typename T>
T* Vector<T>::At(Integer index) {
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
        this->data = rt->ReAllocate<T>(this->data, this->capacity, newCapacity);
        this->capacity = newCapacity;
    }
    T* result = &this->data[this->size.Unwrap()];
    this->size = Integer{this->size.Unwrap() + 1};
    return result;
}

template<typename T>
Integer Vector<T>::Length() const {
    return this->size;
}

CallFrame* Runtime::Frame() {
    Integer length = frames.Length();
    Integer last = Integer{length.Unwrap() - 1};
    return frames.At(last);
}

void CallFrame::Init(Integer stackBase) {
    this->stackBase = stackBase;
    this->programCounter = Integer{0};
    // 1 for the function itself
    this->stackSize = Integer{0};
}

Integer CallFrame::StackTop() const {
    return Integer{this->stackBase.Unwrap() + this->stackSize.Unwrap()};
}

void Value::SetNil() {
    this->type = ValueType::Nil;
    this->as.integer = Integer{0};
}

void Runtime::Interpret() {
    // TODO
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

NativeFunction Value::GetNativeFunction(Runtime* rt) const {
    if (GetType() != ValueType::NativeFunction) {
        rt->PushString("IllegalCast");
        rt->Throw();
        return nullptr;
    }
    return this->as.native;
}

void Runtime::Throw() {
    // TODO
    Panic("RuntimeExceptionThrown");
}

void Runtime::PushString(const char* message) {
    PushNil();
    // TODO: size converstion checking
    Integer length = Integer{static_cast<std::int64_t>(std::strlen(message))};
    char* data = this->New<char>(Integer{length.Unwrap() + 1});
    String* str = this->New<String>(Integer{1});
    str->Init(this->heap, length, data);
    std::memcpy(data, message, length.Unwrap());
    this->heap = str;
    Top()->SetString(str);
}

void Value::SetString(String* str) {
    this->type = ValueType::String;
    this->as.string = str;
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
    this->stack.Push(this)->SetNil();
    Integer prevSize = Frame()->Size();
    Frame()->SetSize(Integer{1 + prevSize.Unwrap()});
}

void String::Init(Object* next, Integer length, char* data) {
    this->ObjectInit(ObjectType::String, next);
    this->length = length;
    this->data = data;
}

void Object::ObjectInit(ObjectType type, Object* next) {
    this->type = type;
    this->next = next;
}

void Runtime::PushFunction() {
    PushNil();
    Function* function = this->New<Function>(Integer{1});
    function->Init(this->heap);
    this->heap = function;
    Top()->SetFunction(function);
}

void Value::SetFunction(Function* fn) {
    this->type = ValueType::Function;
    this->as.function = fn;
}

void Function::Init(Object* next) {
    this->ObjectInit(ObjectType::Function, next);
}

template class Vector<CallFrame>;

}
