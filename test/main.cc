
#include <vector>
#include <functional>
#include <string>
#include <stdexcept>
#include <iostream>

#include "cell.hh"
#include "runtime.hh"

struct TestCase {
    std::string name;
    std::function<void()> runner;
};

void _assertTrue(bool mustBeTrue, const std::string& repr, const std::string& function, std::size_t line);
void _assertThrows(std::function<void()> fn, const std::string& repr, const std::string& function, std::size_t line);

#define assertTrue(c) (_assertTrue(c, #c, __FUNCTION__, __LINE__))

#define assertThrows(c) (_assertThrows(c, #c, __FUNCTION__, __LINE__))

std::vector<TestCase> testCases;

void test(std::string name, std::function<void()> fn) {
    testCases.push_back(TestCase{name, fn});
}

void tests();

int main() {

    // define the tests
    tests();

    // execute them
    std::vector<std::string> failures;

    for (auto& test : testCases) {
        try {
            std::cout << "START " << test.name << "\n";
            test.runner();
            std::cout << "PASS " << test.name << "\n";

        } catch (const std::exception& e) {
            std::cerr << "FAILURE " << test.name << " - " << e.what() << "\n";
            failures.push_back(test.name);
        }
    }

    if (failures.size() == 0) {
        std::cout << "ALL TESTS PASSED\n";
    } else {
        std::cerr << "FAILURES:\n";
        for (const auto& failure : failures) {
            std::cerr << failure << "\n";
        }
    }
}

void _assertTrue(bool mustBeTrue, const std::string& repr, const std::string& function, std::size_t line) {
    if (mustBeTrue) return;
    std::string msg;
    msg.append("ASSERT FAILED: '").append(repr).append("' was false in function ").append(function).append(" on line ").append(std::to_string(line));
    throw std::runtime_error{msg};
}

void _assertThrows(std::function<void()> fn, const std::string& repr, const std::string& function, std::size_t line) {
    bool fail = false;
    try {
        fn();
        fail = true;
    } catch (...) {
        // pass
    }
    if (fail) {
        _assertTrue(false, repr, function, line);
    }
}

void tests() {

    test("default value initialization", []() {
        Value val;
        assertTrue(val.Type() == ValueType::Nil);
    });

    test("value after setting integer", []() {
        Value val;
        val.SetInteger(5);
        assertTrue(val.Type() == ValueType::Integer);
        assertTrue(5 == val.GetInteger());
    });

    test("value after setting cell", []() {
        Value val;
        Cell* cellPtr = reinterpret_cast<Cell*>(&val);
        val.SetCell(cellPtr);
        assertTrue(val.Type() == ValueType::Cell);
        assertTrue(cellPtr == val.GetCell());
    });

    test("test cell on value with nullptr sets to nil", []() {
        Value val;
        val.SetCell(nullptr);
        assertTrue(val.Type() == ValueType::Nil);
    });

    test("incorrect get throws an exception", []() {
        Value val;
        assertThrows([&]() {
            val.GetInteger();
        });
    });

    test("runtime creation", []() {
        DefaultSystem system;
        Runtime runtime{&system};
    });

    test("runtime test pop on empty throws exception", []() {
        DefaultSystem system;
        Runtime runtime{&system};
        assertThrows([&]() {
            runtime.Pop();
        });
    });

    test("runtime new cell pushes cell onto stack", []() {
        DefaultSystem system;
        Runtime runtime{&system};
        runtime.NewCell();
        assertTrue(runtime.Top()->Type() == ValueType::Cell);
        assertTrue(runtime.Top()->GetCell()->First()->Type() == ValueType::Nil);
        assertTrue(runtime.Top()->GetCell()->Second()->Type() == ValueType::Nil);
    });
}