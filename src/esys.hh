#pragma once

#include "edep.hh"

namespace espresso {

class System {
public:
    virtual ~System() = default;

    virtual void* ReAllocate(void* pointer, std::size_t sizeBefore, std::size_t sizeAfter) = 0;

    virtual FILE* Open(const char* name, const char* mode) = 0;

    virtual int Read(FILE* fp) = 0;

    virtual void Close(FILE* fp) = 0;
};

class DefaultSystem : public System {
public:
    DefaultSystem() = default;
    ~DefaultSystem() = default;

    void* ReAllocate(void* pointer, std::size_t sizeBefore, std::size_t sizeAfter) override;

    FILE* Open(const char* name, const char* mode) override;

    int Read(FILE* fp) override;

    void Close(FILE* fp) override;
};

}