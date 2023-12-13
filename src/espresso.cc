#include "espresso.hh"
#include "ert.hh"

namespace espresso {

Espresso::Espresso(System* system) {
    this->impl = system->ReAllocate(nullptr, 0, sizeof(Runtime));
    Runtime* rt = static_cast<Runtime*>(this->impl);
    rt->Init(system);
}

Espresso::~Espresso() {
    Runtime* rt = static_cast<Runtime*>(this->impl);
    System* system = rt->GetSystem();
    rt->DeInit();
    system->ReAllocate(rt, sizeof(Runtime), 0);
}

int Espresso::Load(const char* name) {
    Runtime* rt = static_cast<Runtime*>(this->impl);
    rt->Local(Integer{0})->SetString(rt->NewString("load"));
    rt->LoadGlobal(Integer{0}, Integer{0});
    rt->Local(Integer{1})->SetString(rt->NewString(name));
    try {
        rt->Invoke(Integer{0}, Integer{2});
        return 0;
    } catch (const ThrowException& e) {
        rt->Local(Integer{2})->Copy(rt->StackAtAbsoluteIndex(e.GetAbsoluteStackIndex()));

        rt->Local(Integer{0})->SetString(rt->NewString("print"));
        rt->Local(Integer{1})->SetString(rt->NewString("ERROR Uncaught Exception:"));
        rt->LoadGlobal(Integer{0}, Integer{0});
        rt->Invoke(Integer{0}, Integer{2});

        rt->Local(Integer{0})->SetString(rt->NewString("print"));
        rt->Copy(Integer{1}, Integer{2});
        rt->LoadGlobal(Integer{0}, Integer{0});
        rt->Invoke(Integer{0}, Integer{2});
        return 1;
    }
}

}