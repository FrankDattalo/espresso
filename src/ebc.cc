#include "ert.hh"

namespace espresso {

struct BytecodeReader {

    String* source;
    Integer index;

    uint8_t readU8(Runtime* rt) {
        if (index.Unwrap() >= source->Length().Unwrap()) {
            rt->PushString("File truncated");
            rt->Throw();
            return 0;
        }
        uint8_t result = static_cast<uint8_t>(source->At(index));
        this->index = Integer{index.Unwrap() + 1};
        return result;
    }

    uint16_t readU16(Runtime* rt) {
        uint16_t result = 0;
        result |= readU8(rt);
        result <<= 8;
        result |= readU8(rt);
        return result;
    }

    uint32_t readU32(Runtime* rt) {
        uint32_t result = 0;
        result |= readU16(rt);
        result <<= 16;
        result |= readU16(rt);
        return result;
    }

    uint64_t readU64(Runtime* rt) {
        uint64_t result = 0;
        result |= readU32(rt);
        result <<= 32;
        result |= readU32(rt);
        return result;
    }

    int64_t readI64(Runtime* rt) {
        uint64_t bytes = readU64(rt);
        int64_t* ptr = (int64_t*) &bytes;
        int64_t result = *ptr;
        return result;
    }

    double readF64(Runtime* rt) {
        uint64_t bytes = readU64(rt);
        double* ptr = (double*) &bytes;
        int64_t result = *ptr;
        return result;
    }

    void readConstant(Runtime* rt, Value* dest) {
        uint16_t constantType = readU8(rt);
        switch (constantType) {
            // nil
            case 0: {
                dest->SetNil();
                break;
            }
            // int
            case 1: {
                int64_t integer = readI64(rt);
                dest->SetInteger(Integer{integer});
                break;
            }
            // real
            case 2: {
                double real = readF64(rt);
                dest->SetDouble(Double{real});
                break;
            }
            // string
            case 3: {
                rt->PushString("");
                String* str = rt->Top()->GetString(rt);
                dest->SetString(str);
                uint32_t length = readU32(rt);
                str->Reserve(rt, Integer{length + 1});
                str->Clear();
                for (uint32_t i = 0; i < length; i++) {
                    str->Push(rt, static_cast<char>(readU8(rt)));
                }
                str->Push(rt, '\0');
                rt->Pop();
                break;
            }
            // boolean
            case 4: {
                uint8_t val = readU8(rt);
                dest->SetBoolean(static_cast<bool>(val));
                break;
            }
            // function
            case 5: {
                rt->PushFunction();
                Function* fn = rt->Top()->GetFunction(rt);
                dest->SetFunction(fn);
                readFunction(rt, fn);
                rt->Pop();
                break;
            }
            default: {
                rt->PushString("Invalid constant");
                rt->Throw();
                break;
            }
        }
    }

    void readFunction(Runtime* rt, Function* dest) {
        Integer arity = Integer{readU16(rt)};
        dest->SetArity(arity);
        uint16_t byteCodeCount = readU16(rt);
        dest->ReserveByteCode(rt, Integer{byteCodeCount});
        for (uint16_t i = 0; i < byteCodeCount; i++) {
            ByteCode* bc = dest->PushByteCode(rt);
            bc->Init(rt, readU16(rt));
        }
        uint16_t constantCount = readU16(rt);
        dest->ReserveConstants(rt, Integer{constantCount});
        for (uint16_t i = 0; i < constantCount; i++) {
            Value* constant = dest->PushConstant(rt);
            readConstant(rt, constant);
        }
    }

};

void Runtime::LoadByteCode() {
    // bytecode string -> zero arity function

    this->PushFunction();
    this->Swap();

    // function, bytecode string

    String* string = this->Top()->GetString(this);
    this->Swap();

    // bytecode string, function
    BytecodeReader reader{string, Integer{0}};

    Function* dest = this->Top()->GetFunction(this);

    reader.readFunction(this, dest);

    this->Swap();
    this->Pop();

}

}