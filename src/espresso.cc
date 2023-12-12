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

void Espresso::Load(const char* name) {
    Runtime* rt = static_cast<Runtime*>(this->impl);
    rt->Local(Integer{0})->SetString(rt->NewString("load"));
    rt->LoadGlobal(Integer{0}, Integer{0});
    rt->Local(Integer{1})->SetString(rt->NewString(name));
    rt->Invoke(Integer{0}, Integer{2});
}

}