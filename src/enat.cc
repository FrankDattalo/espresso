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
    {"print", 2, 2, [](Runtime* rt) {
        String* str = rt->Local(Integer{1})->GetString(rt);
        rt->GetSystem()->Write(rt->GetSystem()->Stdout(), str->RawPointer(), str->Length().Unwrap());
        rt->GetSystem()->Write(rt->GetSystem()->Stdout(), "\n", 1);
        rt->Local(Integer{0})->SetNil();
    }}
};

void RegisterNatives(Runtime* rt) {
    for (const Entry& entry : ENTRIES) {
        rt->Local(Integer{0})->SetString(rt->NewString(entry.name));

        rt->Local(Integer{1})->SetNativeFunction(
            rt->NewNativeFunction(
                Integer{entry.arity}, Integer{entry.localCount}, entry.handle));

        rt->DefineGlobal(Integer{0}, Integer{1});
    }

    rt->Local(Integer{0})->SetNil();
}

void Entrypoint(Runtime* rt) {
    rt->Local(Integer{0})->SetString(rt->NewString("readFile"));
    rt->LoadGlobal(Integer{0}, Integer{0});
    rt->Local(Integer{1})->SetString(rt->NewString("lib/entry.bc"));
    rt->Invoke(Integer{0}, Integer{2});
    rt->Copy(Integer{1}, Integer{0});

    rt->Local(Integer{0})->SetString(rt->NewString("readByteCode"));
    rt->LoadGlobal(Integer{0}, Integer{0});
    rt->Invoke(Integer{0}, Integer{2});
}

} // native

} // espresso