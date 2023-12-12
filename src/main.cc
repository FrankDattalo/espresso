#include "espresso.hh"

int main(int argc, char** argv) {

    if (argc != 2) {
        return 1;
    }

    const char* fileName = argv[1];

    espresso::DefaultSystem system;
    espresso::Espresso espresso{&system};
    return espresso.Load(fileName);
}