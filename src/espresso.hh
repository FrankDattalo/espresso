#pragma once

#include "edep.hh"
#include "esys.hh"

namespace espresso {

class Espresso {
public:
    Espresso(System* system, const char* loadPath);
    ~Espresso();

    Espresso(const Espresso&) = delete;
    Espresso& operator=(const Espresso&) = delete;

    Espresso(Espresso&&) = delete;
    Espresso& operator=(Espresso&&) = delete;

    int Load(const char* name);

    int Shell();

private:
    void* impl;
};

}