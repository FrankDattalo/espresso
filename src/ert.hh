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

    void Init(Integer stackBase);

    Value* At(Runtime* rt, Integer index);

    Integer Size() const;
    void SetSize(Integer val);

    Integer AbsoluteStackSize() const;

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

    const T* ConstHeadPointer() const;

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
    Pop,
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

    Integer Argument() const;

    void Init(Runtime* rt, uint16_t value);

private:
    static constexpr uint16_t OP_BITS          = 0b1111100000000000;
    static constexpr uint16_t OP_LOAD_CONSTANT = 0b0000000000000000;
    static constexpr uint16_t OP_LOAD_GLOBAL   = 0b0000100000000000;
    static constexpr uint16_t OP_INVOKE        = 0b0001000000000000;
    static constexpr uint16_t OP_POP           = 0b0001100000000000;
    static constexpr uint16_t OP_RETURN        = 0b0010000000000000;
    static constexpr uint16_t VALUE_BITS       = 0b0000011111111111;

    ByteCodeType type;
    Integer argument{0};
};

typedef void (*NativeFunction)(Runtime* rt);

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
    void SetNativeFunction(NativeFunction value);
    void SetBoolean(bool value);
    void SetMap(Map* val);

    bool Equals(Runtime* rt, Value* other) const;

    ValueType GetType() const;
    void AssertType(Runtime* rt, ValueType type) const;
    Integer GetInteger(Runtime* rt) const;
    Double GetDouble(Runtime* rt) const;
    Function* GetFunction(Runtime* rt) const;
    String* GetString(Runtime* rt) const;
    NativeFunction GetNativeFunction(Runtime* rt) const;
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
        NativeFunction native;
        Map* map;
    } as{Integer{0}};
};

enum class ObjectType {
    String,
    Function,
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

    ByteCode* ByteCodeAt(Integer index);

    Value* ConstantAt(Integer index);

    void SetStack(Integer arity, Integer localCount);

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

    const char* CString(Runtime* rt);

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

    void Invoke(Integer val);

    void PushNil();

    void Pop();

    void Clone();

    Value* Top();

    CallFrame* Frame();

    System* GetSystem();

    void PushString(const char* message);

    void Throw();

    void Interpret();

    Value* StackAt(Integer index);

    void PushFunction();

    void PushMap();

    void PushValue(Value* val);

    void LoadGlobal();

    void ReadFile();

    void LoadByteCode();

    void Swap();

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