#pragma once

#include "edep.hh"
#include "esys.hh"

namespace espresso {

class Espresso {
public:
    Espresso(System* system);
    ~Espresso();

    Espresso(const Espresso&) = delete;
    Espresso& operator=(const Espresso&) = delete;

    Espresso(Espresso&&) = delete;
    Espresso& operator=(Espresso&&) = delete;

    void Invoke(std::int64_t base, std::int64_t argumentCount);

private:
    void* impl;
};

}