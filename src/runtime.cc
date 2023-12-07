#include "runtime.hh"

Runtime::Runtime(System* system) {
    this->system = system;
}

Runtime::~Runtime() {
    while (this->allocations != nullptr) {
        Allocation* toDelete = this->allocations;
        this->allocations = this->allocations->Next();
        this->system->Free(toDelete, sizeof(Allocation));
    }
}

void Runtime::Pop() {
    if (this->stackTop == nullptr) {
        throw std::runtime_error{"Bad Pop - Empty Stack"};
    }
}

Value* Runtime::Top() {
    if (this->stackTop == nullptr) {
        throw std::runtime_error{"Bad Top - Empty Stack"};
    }
    return this->stackTop->First();
}

void Runtime::NewCell() {
    // need 2 cells
    if (this->freeList == nullptr || this->freeList->Second()->Type() == ValueType::Nil) {
        void* dest = system->Allocate(sizeof(Allocation));
        Allocation* alloc = new(dest) Allocation(this->allocations);
        this->allocations = alloc;
        for (std::size_t i = 0; i < this->allocations->CellCount(); i++) {
            Cell* cell = this->allocations->At(i);
            cell->Second()->SetCell(this->freeList);
            cell->First()->SetNil();
            this->freeList = cell;
        }
    }

    Cell* stackCell = this->freeList;
    this->freeList = this->freeList->Second()->GetCell();

    Cell* heapCell = this->freeList;
    this->freeList = this->freeList->Second()->GetCell();

    stackCell->First()->SetCell(heapCell);
    stackCell->Second()->SetCell(this->stackTop);
    this->stackTop = stackCell;

    heapCell->First()->SetNil();
    heapCell->Second()->SetNil();
}

Allocation* Allocation::Next() {
    return next;
}

Allocation::Allocation(Allocation* next_)
: next{next_}
{}

Cell* Allocation::At(std::size_t i) {
    if (i >= CellCount()) {
        throw std::runtime_error{"At"};
    }
    return &cells[i];
}

std::size_t Allocation::CellCount() {
    return CELLS_PER_ALLOCATION;
}