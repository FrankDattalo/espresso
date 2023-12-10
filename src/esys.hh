#pragma once

#include "edep.hh"

namespace espresso {

class System {
public:
    virtual ~System() = default;

    virtual void* ReAllocate(void* pointer, std::size_t sizeBefore, std::size_t sizeAfter) = 0;
};

class DefaultSystem : public System {
public:
    DefaultSystem() = default;
    ~DefaultSystem() = default;

    void* ReAllocate(void* pointer, std::size_t sizeBefore, std::size_t sizeAfter) override;
};

}