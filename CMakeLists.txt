cmake_minimum_required(VERSION 3.15)
project(gba_emu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_executable(gba_emu
    src/main.cpp
    src/cpu/cpu.cpp
    src/memory/bus.cpp
)

target_include_directories(gba_emu PRIVATE src)
target_compile_options(gba_emu PRIVATE -Wall -Wextra)

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)

enable_testing()

add_executable(gba_tests
    tests/bus_test.cpp
    src/memory/bus.cpp
)
target_include_directories(gba_tests PRIVATE src)
target_link_libraries(gba_tests PRIVATE Catch2::Catch2WithMain)
target_compile_options(gba_tests PRIVATE -Wall -Wextra)

include(CTest)
include(Catch)
catch_discover_tests(gba_tests)
