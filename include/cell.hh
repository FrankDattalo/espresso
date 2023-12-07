#ifndef ESPRESSO_CELL_HH_
#define ESPRESSO_CELL_HH_

#include <cstdint>
#include <variant>

class Cell;

enum class ValueType {
  Nil,
  Integer,
  Cell,
};

class Value {
public:
  Value() = default;
  ~Value() = default;
  Value(const Value&) = delete;
  Value& operator=(const Value&) = delete;
  Value(Value&&) = delete;
  Value& operator=(Value&&) = delete;
  std::int64_t GetInteger();
  void SetInteger(std::int64_t val);
  Cell* GetCell();
  void SetCell(Cell* cell);
  void SetNil();
  ValueType Type();
private:
  class Nil{};

  std::variant<Nil, std::int64_t, Cell*> value;
};

class Cell {
public:
  Cell() = default;
  ~Cell() = default;
  Cell(const Cell&) = delete;
  Cell& operator=(const Cell&) = delete;
  Cell(Cell&&) = delete;
  Cell& operator=(Cell&&) = delete;
  Value* First();
  Value* Second();
private:
  Value first;
  Value second;
};

#endif // ESPRESSO_CELL_HH_