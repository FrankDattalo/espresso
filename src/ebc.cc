#include "ert.hh"

namespace espresso {

namespace bytecode {

struct BytecodeReader {

    String* source;
    std::int64_t index;

    uint8_t readU8(Runtime* rt) {
        if (index >= source->Length().Unwrap()) {
            rt->Local(Integer{0})->SetString(rt->NewString("File truncated"));
            rt->Throw(Integer{0});
            return 0;
        }
        uint8_t result = static_cast<uint8_t>(source->At(Integer{index}));
        this->index++;
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
                rt->Local(Integer{2})->SetString(rt->NewString(""));
                String* str = rt->Local(Integer{2})->GetString(rt);
                dest->SetString(str);
                uint32_t length = readU32(rt);
                str->Reserve(rt, Integer{length + 1});
                str->Clear();
                for (uint32_t i = 0; i < length; i++) {
                    str->Push(rt, static_cast<char>(readU8(rt)));
                }
                str->Push(rt, '\0');
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
                rt->Local(Integer{2})->SetFunction(rt->NewFunction());
                Function* fn = rt->Local(Integer{2})->GetFunction(rt);
                dest->SetFunction(fn);
                readFunction(rt, fn);
                break;
            }
            default: {
                rt->Local(Integer{0})->SetString(rt->NewString("Invalid constant"));
                rt->Throw(Integer{0});
                break;
            }
        }
    }

    void readFunction(Runtime* rt, Function* dest) {
        Integer arity = Integer{readU16(rt)};
        Integer localCount = Integer{readU16(rt)};
        dest->SetStack(arity, localCount);
        uint16_t byteCodeCount = readU16(rt);
        dest->ReserveByteCode(rt, Integer{byteCodeCount});
        for (uint16_t i = 0; i < byteCodeCount; i++) {
            ByteCode* bc = dest->PushByteCode(rt);
            bc->Init(rt, readU32(rt));
        }
        uint16_t constantCount = readU16(rt);
        dest->ReserveConstants(rt, Integer{constantCount});
        for (uint16_t i = 0; i < constantCount; i++) {
            Value* constant = dest->PushConstant(rt);
            readConstant(rt, constant);
        }
    }

};

void Load(Runtime* rt) {
    rt->Local(Integer{0})->SetFunction(rt->NewFunction());

    Function* dest = rt->Local(Integer{0})->GetFunction(rt);
    String* string = rt->Local(Integer{1})->GetString(rt);

    BytecodeReader reader{string, 0};

    reader.readFunction(rt, dest);
}

void Verify(Runtime* rt) {
    Function* fn = rt->Local(Integer{1})->GetFunction(rt);
    fn->Verify(rt);
    rt->Copy(Integer{0}, Integer{1});
}

} // bytecode

} // espresso