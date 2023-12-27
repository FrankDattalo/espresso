#include "espresso.hh"

int main(int argc, char** argv) {

    const char* fileName = nullptr;

    if (argc >= 2) {
        fileName = argv[1];
    }

    const char* loadPath = ".";

    espresso::DefaultSystem system;
    espresso::Espresso espresso{&system, loadPath};

    if (fileName != nullptr) {
        return espresso.Load(fileName);
    } else {
        return espresso.Shell();
    }
}