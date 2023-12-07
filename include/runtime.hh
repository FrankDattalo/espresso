#ifndef ESPRESSO_RUNTIME_HH_
#define ESPRESSO_RUNTIME_HH_

#include <stdexcept>

#include "cell.hh"
#include "system.hh"

class Allocation {
public:
    Allocation(Allocation* next_);
    Cell* At(std::size_t index);
    std::size_t CellCount();
    Allocation* Next();
private:
    static constexpr std::size_t CELLS_PER_ALLOCATION = 4096 / sizeof(Cell) - 1;

    Cell cells[CELLS_PER_ALLOCATION];
    Allocation* next;
};

class Runtime;

class PopMarker {
public:
    PopMarker(Runtime* runtime);
    ~PopMarker();
    PopMarker(const PopMarker&) = delete;
    PopMarker& operator=(const PopMarker&) = delete;
    PopMarker(PopMarker&&) = delete;
    PopMarker& operator=(PopMarker&&) = delete;
private:
    Runtime* runtime;
};

class Runtime {
public:
    Runtime(System* system);
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) = delete;
    Runtime& operator=(Runtime&&) = delete;

    void NewCell();
    Value* Top();
    void Pop();
    PopMarker PopMarker();

private:
    Allocation* allocations{nullptr};
    Cell* stackTop{nullptr};
    Cell* freeList{nullptr};
    System* system{nullptr};
};

#endif // ESPRESSO_RUNTIME_HH_