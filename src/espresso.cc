#include "espresso.hh"
#include "ert.hh"

namespace espresso {

Espresso::Espresso(System* system, const char* loadPath) {
    this->impl = system->ReAllocate(nullptr, 0, sizeof(Runtime));
    Runtime* rt = static_cast<Runtime*>(this->impl);
    rt->Init(system, loadPath);
}

Espresso::~Espresso() {
    Runtime* rt = static_cast<Runtime*>(this->impl);
    System* system = rt->GetSystem();
    rt->DeInit();
    system->ReAllocate(rt, sizeof(Runtime), 0);
}

static int unhandledException(Runtime* rt, const ThrowException& e) {
    rt->Local(Integer{2})->Copy(rt->StackAtAbsoluteIndex(e.GetAbsoluteStackIndex()));

    rt->Local(Integer{0})->SetString(rt->NewString("println"));
    rt->Local(Integer{1})->SetString(rt->NewString("ERROR Uncaught Exception:"));
    rt->LoadGlobal(Integer{0}, Integer{0});
    rt->Invoke(Integer{0}, Integer{2});

    rt->Local(Integer{0})->SetString(rt->NewString("println"));
    rt->Copy(Integer{1}, Integer{2});
    rt->LoadGlobal(Integer{0}, Integer{0});
    rt->Invoke(Integer{0}, Integer{2});
    return 1;
}

int Espresso::Shell() {
    Runtime* rt = static_cast<Runtime*>(this->impl);
    rt->Local(Integer{0})->SetString(rt->NewString("shell"));
    rt->LoadGlobal(Integer{0}, Integer{0});
    try {
        rt->Invoke(Integer{0}, Integer{1});
        return 0;
    } catch (const ThrowException& e) {
        return unhandledException(rt, e);
    }
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
        return unhandledException(rt, e);
    }
}

}