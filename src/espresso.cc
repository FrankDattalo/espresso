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

void Espresso::Invoke(std::int64_t base, std::int64_t argumentCount) {
    Runtime* rt = static_cast<Runtime*>(this->impl);
    rt->Invoke(Integer{base}, Integer{argumentCount});
}

}