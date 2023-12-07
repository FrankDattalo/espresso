#include "cell.hh"

Cell* Value::GetCell() {
    return std::get<static_cast<size_t>(ValueType::Cell)>(this->value);
}

std::int64_t Value::GetInteger() {
    return std::get<static_cast<size_t>(ValueType::Integer)>(this->value);
}

ValueType Value::Type() {
    return static_cast<ValueType>(this->value.index());
}

void Value::SetInteger(std::int64_t val) {
    this->value = val;
}

void Value::SetCell(Cell* cell) {
    if (cell == nullptr) {
        SetNil();
        return;
    }
    this->value = cell;
}

void Value::SetNil() {
    this->value = Nil{};
}

Value* Cell::First() {
    return &this->first;
}

Value* Cell::Second() {
    return &this->second;
}