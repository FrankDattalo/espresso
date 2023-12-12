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
    {"verifyByteCode", 2, 4, [](Runtime* rt) {
        espresso::bytecode::Verify(rt);
    }},
    {"print", 2, 2, [](Runtime* rt) {
        Value* toPrint = rt->Local(Integer{1});
        Print(rt, toPrint);
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
    {"load", 2, 2, [](Runtime* rt) {

        rt->Local(Integer{0})->SetString(rt->NewString("readFile"));
        rt->LoadGlobal(Integer{0}, Integer{0});
        // local 1 contains file name
        rt->Invoke(Integer{0}, Integer{2});
        // local 1 contians file contents
        rt->Copy(Integer{1}, Integer{0});

        rt->Local(Integer{0})->SetString(rt->NewString("readByteCode"));
        rt->LoadGlobal(Integer{0}, Integer{0});
        rt->Invoke(Integer{0}, Integer{2});

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

        rt->DefineGlobal(Integer{0}, Integer{1});

        // sanity check
        rt->Local(Integer{1})->GetNativeFunction(rt)->Verify(rt);
    }

    rt->Local(Integer{0})->SetNil();
}

void Print(Runtime* rt, Value* val) {
    
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
        case ValueType::Function: {
            system->Write(out, "fn", 2);
            return;
        }
        case ValueType::NativeFunction: {
            system->Write(out, "fn", 2);
            return;
        }
        case ValueType::String: {
            String* str = val->GetString(rt);
            // TODO: integer type conversion checking
            system->Write(out, str->RawPointer(), str->Length().Unwrap());
            return;
        }
        case ValueType::Boolean: {
            const char* toPrint = (val->GetBoolean(rt)) ? "true" : "false";
            size_t len = std::strlen(toPrint);
            system->Write(out, toPrint, len);
            return;
        }
        case ValueType::Map: {
            system->Write(out, "TODO", 4);
            return;
        }
    }
}

} // native

} // espresso