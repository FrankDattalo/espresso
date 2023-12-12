#include "espresso.hh"

int main() {
    espresso::DefaultSystem system;
    espresso::Espresso espresso{&system};
    espresso.Invoke(0, 1);
    return 0;
}