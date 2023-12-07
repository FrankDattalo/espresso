#include "system.hh"

#include <cstdlib>
#include <new>
#include <string_view>
#include <iostream>

void* DefaultSystem::Allocate(std::size_t sizeInBytes) {
    void* result = malloc(sizeInBytes);
    if (result == nullptr) {
        throw std::bad_alloc{};
    }
    return result;
}

void DefaultSystem::Free(void* pointer, std::size_t sizeInBytes) {
    (void)(sizeInBytes);

    free(pointer);
}

void DefaultSystem::Write(char* msg, std::size_t length) {
    std::string_view view{msg, length};
    std::cout << view;
}

std::size_t DefaultSystem::Read(char* buffer, std::size_t bufferSize) {
    (void)(buffer);
    (void)(bufferSize);

    // TODO
    return 0;
}