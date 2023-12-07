cmake_minimum_required(VERSION 3.19.0)
project(espresso)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED true)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-fsanitize=address,undefined,bounds,alignment -DDEBUG_ENABLED -g3")

set(CMAKE_VERBOSE_MAKEFILE on)

set(COMMON
    src/cell.cc
    src/system.cc
    src/runtime.cc
)

add_executable(espresso ${COMMON} "src/main.cc")
set_target_properties(espresso PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
target_include_directories(espresso PUBLIC include)

include(CTest)
add_executable(unittest ${TESTS} ${COMMON} "test/main.cc")
target_include_directories(unittest PUBLIC include)
target_include_directories(unittest PUBLIC testinclude)
add_test(NAME unittest COMMAND unittest)
set_property(TEST unittest PROPERTY PASS_REGULAR_EXPRESSION "ALL TESTS PASSED")
