#include "espresso.hh"

int main(int argc, char** argv) {

    if (argc != 3) {
        return 1;
    }

    const char* loadPath = argv[1];
    const char* fileName = argv[2];

    espresso::DefaultSystem system;
    espresso::Espresso espresso{&system, loadPath};
    return espresso.Load(fileName);
}