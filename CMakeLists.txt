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
