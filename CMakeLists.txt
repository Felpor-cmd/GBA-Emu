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
    src/cpu/cpu_branch.cpp
    src/cpu/cpu_data_processing.cpp
    src/cpu/cpu_memory.cpp
    src/cpu/cpu_psr.cpp
    src/cpu/cpu_swi.cpp
    src/cpu/cpu_thumb.cpp
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
    test/main_test.cpp
    test/bus/bus_test_ewram_read_write.cpp
    test/bus/bus_test_iwram_read_write.cpp
    test/bus/bus_test_palette_ram_read_write.cpp
    test/bus/bus_test_vram_read_write.cpp
    test/bus/bus_test_oam_read_write.cpp
    test/bus/bus_test_bios_read_zeroed_placeholder_writes_ignored.cpp
    test/bus/bus_test_io_register_placeholder_read_write.cpp
    test/bus/bus_test_rom_read_all_three_mirrored_address_windows_bounds_checked.cpp
    test/bus/bus_test_sram_read_write.cpp
    test/cpu/cpu_test_condition_code_evaluation.cpp
    test/cpu/cpu_test_fetch_decode_skeleton.cpp
    test/cpu/cpu_test_branch_fetch_width.cpp
    test/cpu/cpu_test_data_processing_register_operand.cpp
    test/cpu/cpu_test_data_processing_shifted_register_operand.cpp
    test/cpu/cpu_test_branch_and_exchange.cpp
    test/cpu/cpu_test_single_data_transfer.cpp
    test/cpu/cpu_test_halfword_and_signed_transfers.cpp
    test/cpu/cpu_test_block_data_transfer.cpp
    test/cpu/cpu_test_multiply.cpp
    test/cpu/cpu_test_psr_transfer.cpp
    test/cpu/cpu_test_swi.cpp
    test/cpu/cpu_test_thumb_shift.cpp
    src/cpu/cpu.cpp
    src/cpu/cpu_branch.cpp
    src/cpu/cpu_data_processing.cpp
    src/cpu/cpu_memory.cpp
    src/cpu/cpu_psr.cpp
    src/cpu/cpu_swi.cpp
    src/cpu/cpu_thumb.cpp
    src/memory/bus.cpp
)
target_include_directories(gba_tests PRIVATE src)
target_link_libraries(gba_tests PRIVATE Catch2::Catch2)
target_compile_options(gba_tests PRIVATE -Wall -Wextra)

include(CTest)
include(Catch)
catch_discover_tests(gba_tests)
