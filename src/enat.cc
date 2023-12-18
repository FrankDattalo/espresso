#include "enat.hh"
#include "ert.hh"
#include "ebc.hh"

namespace espresso {

namespace native {

struct Entry {
    const char* name;
    std::int64_t arity;
    std::int64_t localCount;
    NativeFunction::Handle handle;
};

void Print(Runtime* rt, Value* toPrint);

static constexpr Entry ENTRIES[] = {
    {"readFile", 2, 2, [](Runtime* rt) {
        String* fileName = rt->Local(Integer{1})->GetString(rt);
        const char* fileNameCString = fileName->RawPointer();
        FILE* fp = rt->GetSystem()->Open(fileNameCString, "rb");
        if (fp == nullptr) {
            rt->Local(Integer{0})->SetString(rt->NewString("Could not open file"));
            rt->Throw(Integer{0});
            return;
        }
        rt->Local(Integer{0})->SetString(rt->NewString(""));
        String* dest = rt->Local(Integer{0})->GetString(rt);
        int c = EOF;
        dest->Clear();
        while ((c = rt->GetSystem()->Read(fp)) != EOF) {
            dest->Push(rt, static_cast<char>(c));
        }
        rt->GetSystem()->Close(fp);
        dest->Push(rt, '\0');
    }},
    {"readByteCode", 2, 3, [](Runtime* rt) {
        espresso::bytecode::Load(rt);
    }},
    {"compile", 2, 4, [](Runtime* rt) {
        espresso::native::compiler::Compile(rt);
    }},
    {"verifyByteCode", 2, 4, [](Runtime* rt) {
        espresso::bytecode::Verify(rt);
    }},
    {"print", 2, 2, [](Runtime* rt) {
        Print(rt, rt->Local(Integer{1}));
        rt->GetSystem()->Write(rt->GetSystem()->Stdout(), "\n", 1);
        rt->Local(Integer{0})->SetNil();
    }},
    {"try", 2, 3, [](Runtime* rt) {
        Integer result{0};
        const char* key = "result";
        try {
            rt->Invoke(Integer{1}, Integer{1});
            result = rt->CurrentFrame()->AbsoluteIndex(Integer{1});
        } catch (const ThrowException& e) {
            result = e.GetAbsoluteStackIndex();
            key = "error";
        }
        rt->Local(Integer{2})->SetMap(rt->NewMap());
        rt->Local(Integer{0})->SetString(rt->NewString(key));
        Map* map = rt->Local(Integer{2})->GetMap(rt);
        map->Put(rt, rt->Local(Integer{0}), rt->StackAtAbsoluteIndex(result));
        rt->Copy(Integer{0}, Integer{2});
    }},
    {"endsWith", 3, 3, [](Runtime* rt) {
        String* haystack = rt->Local(Integer{1})->GetString(rt);
        String* needle = rt->Local(Integer{2})->GetString(rt);

        if (needle->Length().Unwrap() > haystack->Length().Unwrap()) {
            rt->Local(Integer{0})->SetBoolean(false);
            return;
        }

        std::int64_t needleIndex = needle->Length().Unwrap() - 1;
        std::int64_t haystackIndex = haystack->Length().Unwrap() - 1;

        while (needleIndex >= 0) {

            char needleCurr = needle->At(Integer{needleIndex});
            char haystackCurr = haystack->At(Integer{haystackIndex});

            if (needleCurr != haystackCurr) {
                rt->Local(Integer{0})->SetBoolean(false);
                return;
            }

            needleIndex--;
            haystackIndex--;
        }

        rt->Local(Integer{0})->SetBoolean(true);
        return;
    }},
    {"load", 2, 5, [](Runtime* rt) {

        rt->Local(Integer{2})->SetString(rt->NewString("endsWith"));
        rt->LoadGlobal(Integer{2}, Integer{2});
        rt->Copy(Integer{3}, Integer{1});
        rt->Local(Integer{4})->SetString(rt->NewString(".espresso"));
        rt->Invoke(Integer{2}, Integer{3});
        bool isSourceFile = rt->Local(Integer{2})->GetBoolean(rt);

        // TODO: load from load path
        // String* requestedFile = rt->Local(Integer{1})->GetString(rt);
        // rt->Local(Integer{4})->SetString(rt->NewString(""));
        // String* str = rt->Local(Integer{4})->GetString(rt);
        // 

        rt->Local(Integer{0})->SetString(rt->NewString("readFile"));
        rt->LoadGlobal(Integer{0}, Integer{0});
        // local 1 contains file name
        rt->Invoke(Integer{0}, Integer{2});
        // local 1 contians file contents
        rt->Copy(Integer{1}, Integer{0});

        if (isSourceFile) {
            rt->Local(Integer{0})->SetString(rt->NewString("compile"));
            rt->LoadGlobal(Integer{0}, Integer{0});
            rt->Invoke(Integer{0}, Integer{2});
        } else {
            rt->Local(Integer{0})->SetString(rt->NewString("readByteCode"));
            rt->LoadGlobal(Integer{0}, Integer{0});
            rt->Invoke(Integer{0}, Integer{2});
        }

        rt->Copy(Integer{1}, Integer{0});
        rt->Local(Integer{0})->SetString(rt->NewString("verifyByteCode"));
        rt->LoadGlobal(Integer{0}, Integer{0});
        rt->Invoke(Integer{0}, Integer{2});

        // local 0 contains bytecode
        rt->Invoke(Integer{0}, Integer{1});
        // invoke the bytecode
    }},
    {"throw", 2, 2, [](Runtime* rt) {
        Integer absoluteIndex = rt->CurrentFrame()->AbsoluteIndex(Integer{1});
        throw ThrowException{absoluteIndex};
    }},
};

void RegisterNatives(Runtime* rt) {
    for (const Entry& entry : ENTRIES) {
        rt->Local(Integer{0})->SetString(rt->NewString(entry.name));

        rt->Local(Integer{1})->SetNativeFunction(
            rt->NewNativeFunction(
                Integer{entry.arity}, Integer{entry.localCount}, entry.handle));

        rt->StoreGlobal(Integer{0}, Integer{1});

        // sanity check
        rt->Local(Integer{1})->GetNativeFunction(rt)->Verify(rt);
    }

    rt->Local(Integer{0})->SetNil();
}

struct Printed {
    Object* object;
    Printed* next;

    bool Contains(Object* obj) {
        Printed* curr = this;
        while (curr != nullptr) {
            if (obj == curr->object) {
                return true;
            }
            curr = curr->next;
        }
        return false;
    }
};

static void DoPrint(Runtime* rt, Value* val, Printed* printed, bool display) {

    constexpr static size_t SPRINTF_BUFFER_SIZE = 66;

    System* system = rt->GetSystem();
    FILE* out = system->Stdout();

    switch (val->GetType()) {
        case ValueType::Nil: {
            system->Write(out, "nil", 3);
            return;
        }
        case ValueType::Integer: {
            char buffer[SPRINTF_BUFFER_SIZE];
            std::int64_t current = val->GetInteger(rt).Unwrap();
            std::snprintf(buffer, SPRINTF_BUFFER_SIZE, "%lld", current);
            size_t len = std::strlen(buffer);
            system->Write(out, buffer, len);
            return;
        }
        case ValueType::Double: {
            char buffer[SPRINTF_BUFFER_SIZE];
            double current = val->GetDouble(rt).Unwrap();
            std::snprintf(buffer, SPRINTF_BUFFER_SIZE, "%lf", current);
            size_t len = std::strlen(buffer);
            system->Write(out, buffer, len);
            return;
        }
        case ValueType::NativeFunction:
        case ValueType::Function: {
            system->Write(out, "{fn}", 4);
            return;
        }
        case ValueType::String: {
            String* str = val->GetString(rt);
            // TODO: integer type conversion checking
            if (display) {
                system->Write(out, "\"", 1);
            }
            system->Write(out, str->RawPointer(), str->Length().Unwrap());
            if (display) {
                system->Write(out, "\"", 1);
            }
            return;
        }
        case ValueType::Boolean: {
            const char* toPrint = (val->GetBoolean(rt)) ? "true" : "false";
            size_t len = std::strlen(toPrint);
            system->Write(out, toPrint, len);
            return;
        }
        case ValueType::Map: {
            Map* map = val->GetMap(rt);
            if (printed != nullptr && printed->Contains(map)) {
                const char* msg = "{recursive}";
                system->Write(out, msg, std::strlen(msg));
                return;
            }

            Printed newPrinted = Printed{map, printed};
            printed = &newPrinted;

            system->Write(out, "{", 1);

            bool first = true;
            Map::Iterator iter = map->GetIterator();
            while (iter.HasNext()) {
                if (!first) {
                    system->Write(out, ", ", 2);
                }
                DoPrint(rt, iter.Key(), printed, true);
                system->Write(out, " = ", 3);
                DoPrint(rt, iter.Value(), printed, true);
                first = false;
            }

            system->Write(out, "}", 1);
            return;
        }
    }
}

void Print(Runtime* rt, Value* val) {
    DoPrint(rt, val, nullptr, false);
}

namespace debugger {

void PrintByteCode(Runtime* rt, Function* fn, ByteCode* bc) {
    static constexpr const char* fmt = "%-8s";
    switch (bc->Type()) {
        case ByteCodeType::NoOp: {
            std::printf(fmt, "noop");
            std::printf("\n");
            break;
        }
        case ByteCodeType::Return: {
            std::printf(fmt, "return");
            std::printf("R%lld", bc->SmallArgument1().Unwrap());
            std::printf("\n");
            break;
        }
        case ByteCodeType::LoadConstant: {
            std::printf(fmt, "loadc");
            std::printf("R%lld ", bc->SmallArgument1().Unwrap());
            DoPrint(rt, fn->ConstantAt(bc->LargeArgument()), nullptr, true);
            std::printf("\n");
            break;
        }
        case ByteCodeType::LoadGlobal: {
            std::printf(fmt, "loadg");
            std::printf(
                "R%lld R%lld\n",
                bc->SmallArgument1().Unwrap(), bc->SmallArgument2().Unwrap());
            break;
        }
        case ByteCodeType::Invoke: {
            std::printf(fmt, "invoke");
            std::printf(
                "R%lld %lld\n",
                bc->SmallArgument1().Unwrap(), bc->SmallArgument2().Unwrap());
            break;
        }
        case ByteCodeType::Copy: {
            std::printf(fmt, "copy");
            std::printf(
                "R%lld R%lld\n",
                bc->SmallArgument1().Unwrap(), bc->SmallArgument2().Unwrap());
            break;
        }
        case ByteCodeType::Equal: {
            std::printf(fmt, "equal");
            std::printf(
                "R%lld R%lld R%lld\n",
                bc->SmallArgument1().Unwrap(), bc->SmallArgument2().Unwrap(), bc->SmallArgument3().Unwrap());
            break;
        }
        case ByteCodeType::Add: {
            std::printf(fmt, "add");
            std::printf(
                "R%lld R%lld R%lld\n",
                bc->SmallArgument1().Unwrap(), bc->SmallArgument2().Unwrap(), bc->SmallArgument3().Unwrap());
            break;
        }
        case ByteCodeType::Subtract: {
            std::printf(fmt, "sub");
            std::printf(
                "R%lld R%lld R%lld\n",
                bc->SmallArgument1().Unwrap(), bc->SmallArgument2().Unwrap(), bc->SmallArgument3().Unwrap());
            break;
        }
        case ByteCodeType::Multiply: {
            std::printf(fmt, "mult");
            std::printf(
                "R%lld R%lld R%lld\n",
                bc->SmallArgument1().Unwrap(), bc->SmallArgument2().Unwrap(), bc->SmallArgument3().Unwrap());
            break;
        }
        case ByteCodeType::JumpIfFalse: {
            std::printf(fmt, "jumpf");
            std::printf(
                "R%lld %lld\n",
                bc->SmallArgument1().Unwrap(), bc->LargeArgument().Unwrap());
            break;
        }
        case ByteCodeType::Jump: {
            std::printf(fmt, "jump");
            std::printf("%lld\n", bc->LargeArgument().Unwrap());
            break;
        }
        case ByteCodeType::StoreGlobal: {
            std::printf(fmt, "storeg");
            std::printf(
                "R%lld R%lld\n",
                bc->SmallArgument1().Unwrap(), bc->SmallArgument2().Unwrap());
            break;
        }
        case ByteCodeType::Not: {
            std::printf(fmt, "not");
            std::printf(
                "R%lld\n",
                bc->SmallArgument1().Unwrap());
            break;
        }
        case ByteCodeType::MapSet: {
            std::printf(fmt, "mapset");
            std::printf(
                "R%lld R%lld R%lld\n",
                bc->SmallArgument1().Unwrap(),
                bc->SmallArgument2().Unwrap(),
                bc->SmallArgument3().Unwrap());
            break;
        }
        case ByteCodeType::NewMap: {
            std::printf(fmt, "newmap");
            std::printf(
                "R%lld\n",
                bc->SmallArgument1().Unwrap());
            break;
        }
        default: {
            Panic("Unhandled ByteCodeType");
        }
    }

}

void Breakpoint(Runtime* runtime) {
    std::printf("\033[2J\033[1;1H");
    std::int64_t framesCount = runtime->FrameCount().Unwrap();
    for (std::int64_t i = framesCount - 1; i >= 0; i--) {
        std::printf("[%lld]", i);
        CallFrame* frame = runtime->FrameAt(Integer{i});
        std::int64_t frameSize = frame->Size().Unwrap();
        for (std::int64_t j = 0; j < frameSize; j++) {
            Value* val = frame->At(runtime, Integer{j});
            std::printf(" ");
            espresso::native::DoPrint(runtime, val, nullptr, true);
        }
        std::printf("\n");
    }
    std::printf("\n");

    CallFrame* frame = runtime->CurrentFrame();
    Function* fn = frame->At(runtime, Integer{0})->GetFunction(runtime);
    std::int64_t bytecodeCount = fn->GetByteCodeCount().Unwrap();
    std::int64_t currentByteCode = frame->ProgramCounter().Unwrap();

    for (std::int64_t i = 0; i < bytecodeCount; i++) {
        std::printf("[%03lld] ", i);
        if (i == currentByteCode) {
            std::printf(">> ");
        } else {
            std::printf("   ");
        }
        ByteCode* bc = fn->ByteCodeAt(Integer{i});
        PrintByteCode(runtime, fn, bc);
    }

    std::getc(stdin);
}

} // debugger

} // native

} // espresso