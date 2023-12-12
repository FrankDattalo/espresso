#pragma once

#include "edep.hh"
#include "esys.hh"
#include "espresso.hh"

namespace espresso {

template<typename T>
class Numeric {
public:
    Numeric() = default;

    explicit Numeric(T val) : value{val} {}

    ~Numeric() = default;

    Numeric(const Numeric&) = default;
    Numeric& operator=(const Numeric&) = default;

    Numeric(Numeric&&) = default;
    Numeric& operator=(Numeric&&) = default;

    T Unwrap() const { return value; }

private:
    T value{0};
};

using Integer = Numeric<std::int64_t>;

using Double = Numeric<double>;

class Runtime;
class Value;

class PanicException : public std::exception {
public:
    PanicException(const char* message_);
    virtual ~PanicException() = default;

    PanicException(const PanicException&) = default;
    PanicException& operator=(const PanicException&) = default;

    PanicException(PanicException&&) = default;
    PanicException& operator=(PanicException&&) = default;

    const char* what() const noexcept override;

private:
    const char* message;
};

void Panic(const char* message);

class ThrowException : public std::exception {
public:
    ThrowException(Integer stackIndex);
    virtual ~ThrowException() = default;

    ThrowException(const ThrowException&) = default;
    ThrowException& operator=(const ThrowException&) = default;

    ThrowException(ThrowException&&) = default;
    ThrowException& operator=(ThrowException&&) = default;

    const char* what() const noexcept override;
    
private:
    Integer stackIndex;
};

class CallFrame {
public:
    CallFrame() = default;
    ~CallFrame() = default;

    CallFrame(const CallFrame&) = delete;
    CallFrame& operator=(const CallFrame&) = delete;

    CallFrame(CallFrame&&) = delete;
    CallFrame& operator=(CallFrame&&) = delete;

    void Init(Integer stackBase, Integer argumentCount);

    Value* At(Runtime* rt, Integer index);

    Integer AbsoluteIndex(Integer localNumber) const;

    Integer ProgramCounter() const;
    void AdvanceProgramCounter();

private:
    Integer stackBase{0};
    Integer stackSize{0};
    Integer programCounter{0};
};

template<typename T>
class Vector {
public:
    Vector() = default;
    ~Vector() = default;

    Vector(const Vector&) = delete;
    Vector& operator=(const Vector&) = delete;

    Vector(Vector&&) = delete;
    Vector& operator=(Vector&&) = delete;

    void Init(Runtime* rt);

    void InitWithCapacity(Runtime* rt, Integer capacity);

    void DeInit(Runtime* rt);

    T* Push(Runtime* rt);

    void Pop();

    T* At(Integer index) const;

    Integer Length() const;

    void Reserve(Runtime* rt, Integer cap);

    const T* RawHeadPointer() const;

    void Truncate(Integer newLength);

private:
    T* data{nullptr};
    Integer size{0};
    Integer capacity{0};
};

enum class ValueType {
    Nil,
    Integer,
    Double,
    Function,
    NativeFunction,
    String,
    Boolean,
    Map,
};

enum class ByteCodeType {
    NoOp,
    Return,
    LoadConstant,
    LoadGlobal,
    Invoke,
};

class ByteCode {
public:
    ByteCode() = default;
    ~ByteCode() = default;

    ByteCode(const ByteCode&) = delete;
    ByteCode& operator=(const ByteCode&) = delete;

    ByteCode(ByteCode&&) = delete;
    ByteCode& operator=(ByteCode&&) = delete;

    ByteCodeType Type() const;

    Integer Argument1() const;
    Integer Argument2() const;

    void Init(Runtime* rt, uint32_t value);

private:

    static constexpr uint32_t ARG_BITS_COUNT   = 12;
    static constexpr uint32_t OP_BITS          = 0b11111111000000000000000000000000;
    static constexpr uint32_t ARG1_BITS        = 0b00000000111111111111000000000000;
    static constexpr uint32_t ARG2_BITS        = 0b00000000000000000000111111111111;
    static constexpr uint32_t OP_LOAD_CONSTANT = 0b00000001000000000000000000000000;
    static constexpr uint32_t OP_LOAD_GLOBAL   = 0b00000010000000000000000000000000;
    static constexpr uint32_t OP_INVOKE        = 0b00000011000000000000000000000000;
    static constexpr uint32_t OP_RETURN        = 0b00000100000000000000000000000000;

    ByteCodeType type{ByteCodeType::NoOp};
    Integer argument1{0};
    Integer argument2{0};
};


class NativeFunction;
class Function;
class String;
class Map;

class Value {
public:
    Value() = default;
    ~Value() = default;

    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;

    Value(Value&&) = delete;
    Value& operator=(Value&&) = delete;

    void SetNil();
    void SetInteger(Integer value);
    void SetDouble(Double value);
    void SetFunction(Function* value);
    void SetString(String* value);
    void SetNativeFunction(NativeFunction* value);
    void SetBoolean(bool value);
    void SetMap(Map* val);

    bool Equals(Runtime* rt, Value* other) const;

    ValueType GetType() const;
    void AssertType(Runtime* rt, ValueType type) const;
    Integer GetInteger(Runtime* rt) const;
    Double GetDouble(Runtime* rt) const;
    Function* GetFunction(Runtime* rt) const;
    String* GetString(Runtime* rt) const;
    NativeFunction* GetNativeFunction(Runtime* rt) const;
    bool GetBoolean(Runtime* rt) const;
    Map* GetMap(Runtime* rt) const;

    void Copy(Runtime* rt, Value* other);

private:
    ValueType type{ValueType::Nil};
    union {
        Integer integer;
        Double real;
        Function* function;
        String* string;
        NativeFunction* nativeFunction;
        Map* map;
    } as{Integer{0}};
};

enum class ObjectType {
    String,
    Function,
    NativeFunction,
    Map,
};

class Object {
public:
    Object() = default;
    ~Object() = default;

    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    Object(Object&&) = delete;
    Object& operator=(Object&&) = delete;

    ObjectType Type() const;

    void ObjectInit(ObjectType type, Object* next);

private:
    ObjectType type;
    Object* next;
};

class Function : public Object {
public:
    Function() = default;
    ~Function() = default;

    Function(const Function&) = delete;
    Function& operator=(const Function&) = delete;

    Function(Function&&) = delete;
    Function& operator=(Function&&) = delete;

    void Init(Runtime* rt, Object* next);

    ByteCode* ByteCodeAt(Integer index) const;

    Value* ConstantAt(Integer index) const;

    void SetStack(Integer arity, Integer localCount);

    Integer GetArity() const;

    Integer GetLocalCount() const;

    void ReserveByteCode(Runtime* rt, Integer capacity);

    void ReserveConstants(Runtime* rt, Integer capacity);

    ByteCode* PushByteCode(Runtime* rt);

    Value* PushConstant(Runtime* rt);

private:
    Integer arity{0};
    Integer localCount{0};
    Vector<ByteCode> byteCode;
    Vector<Value> constants;
};

class NativeFunction : public Object {
public:
    using Handle = void (*)(Runtime*);

    NativeFunction() = default;
    ~NativeFunction() = default;

    NativeFunction(const NativeFunction&) = delete;
    NativeFunction& operator=(const NativeFunction&) = delete;

    NativeFunction(NativeFunction&&) = delete;
    NativeFunction& operator=(NativeFunction&&) = delete;

    void Init(Runtime* rt, Object* next, Integer arity, Integer localCount, Handle handle);

    Integer GetArity() const;

    Integer GetLocalCount() const;

    Handle GetHandle() const;

private:
    Integer arity{0};
    Integer localCount{0};
    Handle handle{nullptr};
};

class String : public Object {
public:
    String() = default;
    ~String() = default;

    String(const String&) = delete;
    String& operator=(const String&) = delete;

    String(String&&) = delete;
    String& operator=(String&&) = delete;

    void Init(Runtime* rt, Object* next, Integer length, const char* data);

    bool Equals(String* other) const;

    Integer Length() const;

    char At(Integer index) const;

    void Push(Runtime* rt, char c);

    void Reserve(Runtime* rt, Integer val);

    const char* RawPointer() const;

    void Clear();

private:
    Vector<char> data;
};

class Map : public Object {
public:
    Map() = default;
    ~Map() = default;

    Map(const Map&) = delete;
    Map& operator=(const Map&) = delete;

    Map(Map&&) = delete;
    Map& operator=(Map&&) = delete;

    void Init(Runtime* rt, Object* next);

    Value* Get(Runtime* rt, Value* key);

    void Put(Runtime* rt, Value* key, Value* value);

private:
    class Entry {
    friend class Map;
        Value key;
        Value value;
    };

    Vector<Entry> entries;
};

class Runtime {
public:
    Runtime() = default;
    ~Runtime() = default;

    void Init(System* system);
    void DeInit();

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    Runtime(Runtime*&) = delete;
    Runtime& operator=(Runtime&&) = delete;

    void Invoke(Integer base, Integer argumentCount);

    CallFrame* CurrentFrame();

    System* GetSystem();

    void Interpret();

    Value* StackAtAbsoluteIndex(Integer index);

    Value* Local(Integer index);

    Function* NewFunction();

    Map* NewMap();

    String* NewString(const char* data);

    NativeFunction* NewNativeFunction(Integer arity, Integer localCount, NativeFunction::Handle handle);

    void Throw(Integer localNumber);

    void LoadGlobal(Integer destLocalNumber, Integer sourceLocalNumber);

    void LoadConstant(Integer destLocalNumber, Integer sourceConstantNumber);

    void DefineGlobal(Integer keyLocalNumber, Integer valueLocalNumber);

    void Copy(Integer destIndex, Integer sourceIndex);

    void Return(Integer sourceIndex);

    template<typename T>
    T* New(Integer length);

    template<typename T>
    T* ReAllocate(T* ptr, Integer prevLength, Integer length);

    template<typename T>
    void Free(T* ptr, Integer length);

private:

    System* system{nullptr};
    Vector<CallFrame> frames;
    Vector<Value> stack;
    Map* globals{nullptr};
    Object* heap{nullptr};
};

}