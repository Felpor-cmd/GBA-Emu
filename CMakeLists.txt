cmake_minimum_required(VERSION 3.15)
project(gba_emu LANGUAGES CXX)

find_package(SDL2 REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_executable(gba_emu
    src/main.cpp
    src/emulator.cpp
    src/cpu/cpu.cpp
    src/cpu/cpu_branch.cpp
    src/cpu/cpu_data_processing.cpp
    src/cpu/cpu_memory.cpp
    src/cpu/cpu_psr.cpp
    src/cpu/cpu_swi.cpp
    src/cpu/cpu_thumb.cpp
    src/cpu/cpu_exception.cpp
    src/ppu/ppu.cpp
    src/memory/bus.cpp
)

target_include_directories(gba_emu PRIVATE src)
target_link_libraries(gba_emu PRIVATE SDL2::SDL2)
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
    test/emulator_test.cpp
    src/emulator.cpp
    test/bus/bus_test_ewram_read_write.cpp
    test/bus/bus_test_iwram_read_write.cpp
    test/bus/bus_test_palette_ram_read_write.cpp
    test/bus/bus_test_vram_read_write.cpp
    test/bus/bus_test_oam_read_write.cpp
    test/bus/bus_test_bios_read_zeroed_placeholder_writes_ignored.cpp
    test/bus/bus_test_io_register_placeholder_read_write.cpp
    test/bus/bus_test_rom_read_all_three_mirrored_address_windows_bounds_checked.cpp
    test/bus/bus_test_sram_read_write.cpp
    test/bus/bus_test_dma.cpp
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
    test/cpu/cpu_test_bios_boot.cpp
    test/cpu/cpu_test_thumb_shift.cpp
    test/cpu/cpu_test_thumb_families.cpp
    test/cpu/cpu_test_single_data_swap.cpp
    test/cpu/cpu_test_user_mode_transfer.cpp
    test/cpu/cpu_test_coprocessor.cpp
    test/cpu/cpu_test_pc_behavior.cpp
    test/cpu/cpu_test_memory_edge_cases.cpp
    test/cpu/cpu_test_timing_and_decode.cpp
    test/cpu/cpu_test_arm_fixtures.cpp
    test/ppu/ppu_test_bitmap.cpp
    test/ppu/ppu_test_tiles.cpp
    test/ppu/ppu_test_effects.cpp
    src/cpu/cpu.cpp
    src/cpu/cpu_branch.cpp
    src/cpu/cpu_data_processing.cpp
    src/cpu/cpu_memory.cpp
    src/cpu/cpu_psr.cpp
    src/cpu/cpu_swi.cpp
    src/cpu/cpu_thumb.cpp
    src/cpu/cpu_exception.cpp
    src/ppu/ppu.cpp
    src/memory/bus.cpp
)
target_include_directories(gba_tests PRIVATE src)
target_compile_definitions(gba_tests PRIVATE FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/arm")
target_link_libraries(gba_tests PRIVATE Catch2::Catch2)
target_compile_options(gba_tests PRIVATE -Wall -Wextra)

include(CTest)
include(Catch)
catch_discover_tests(gba_tests)

add_executable(opcode_rom_runner
    test/reference/opcode_rom_runner.cpp
    src/cpu/cpu.cpp src/cpu/cpu_branch.cpp src/cpu/cpu_data_processing.cpp
    src/cpu/cpu_memory.cpp src/cpu/cpu_psr.cpp src/cpu/cpu_swi.cpp
    src/cpu/cpu_thumb.cpp src/cpu/cpu_exception.cpp src/memory/bus.cpp)
target_include_directories(opcode_rom_runner PRIVATE src)
target_compile_options(opcode_rom_runner PRIVATE -Wall -Wextra)
add_test(NAME opcode_arm COMMAND opcode_rom_runner
    "${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/opcode/jsmolka/arm.gba" 0x08001D4C 12 8824)
add_test(NAME opcode_thumb COMMAND opcode_rom_runner
    "${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/opcode/jsmolka/thumb.gba" 0x08000934 7 3680)

if(DEFINED MGBA_ROOT)
    find_path(MGBA_INCLUDE_DIR mgba/core/core.h PATHS "${MGBA_ROOT}/include" NO_DEFAULT_PATH REQUIRED)
    find_library(MGBA_LIBRARY mgba PATHS "${MGBA_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
    add_executable(mgba_frame_capture test/reference/mgba_frame_capture.cpp)
    target_include_directories(mgba_frame_capture PRIVATE "${MGBA_INCLUDE_DIR}")
    target_link_libraries(mgba_frame_capture PRIVATE "${MGBA_LIBRARY}")
    add_executable(mgba_trace_compare
        test/reference/mgba_trace_compare.cpp
        src/cpu/cpu.cpp src/cpu/cpu_branch.cpp src/cpu/cpu_data_processing.cpp
        src/cpu/cpu_memory.cpp src/cpu/cpu_psr.cpp src/cpu/cpu_swi.cpp
        src/cpu/cpu_thumb.cpp src/cpu/cpu_exception.cpp src/memory/bus.cpp)
    target_include_directories(mgba_trace_compare PRIVATE src "${MGBA_INCLUDE_DIR}")
    target_link_libraries(mgba_trace_compare PRIVATE "${MGBA_LIBRARY}")
    add_test(NAME mgba_arm_trace COMMAND mgba_trace_compare
        "${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/arm/sequence.gba" 9)
    add_test(NAME mgba_arm_multiply_block_trace COMMAND mgba_trace_compare
        "${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/arm/multiply_block.gba" 11)
    add_test(NAME mgba_thumb_arithmetic_trace COMMAND mgba_trace_compare
        "${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/arm/thumb_arithmetic.gba" 16)
    add_test(NAME mgba_thumb_memory_trace COMMAND mgba_trace_compare
        "${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/arm/thumb_memory.gba" 22)
endif()

set(GBA_VERIFY_ROMS "" CACHE STRING "Optional semicolon-separated list of local GBA ROMs to compare with mGBA")
set(GBA_VERIFY_FRAMES "600,3600" CACHE STRING "Comma-separated frame checkpoints for local ROM verification")
set(GBA_VERIFY_MIN_COLORS "1" CACHE STRING "Minimum unique 5-bit colors required in local ROM captures")
set(GBA_ZELDA_ROM "" CACHE FILEPATH "Optional local Zelda ROM (compatibility alias; adds a pinned Zelda check)")
if(GBA_VERIFY_ROMS OR GBA_ZELDA_ROM)
    if(NOT DEFINED MGBA_ROOT)
        message(FATAL_ERROR "Local ROM verification requires MGBA_ROOT for reference captures")
    endif()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(_gba_verify_roms ${GBA_VERIFY_ROMS})
    if(GBA_ZELDA_ROM)
        get_filename_component(_zelda_rom_abs "${GBA_ZELDA_ROM}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    if(GBA_ZELDA_ROM)
        list(APPEND _gba_verify_roms "${GBA_ZELDA_ROM}")
    endif()
    set(_gba_verify_names)
    set(_gba_verify_paths)
    foreach(_rom IN LISTS _gba_verify_roms)
        get_filename_component(_rom_abs "${_rom}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(FIND _gba_verify_paths "${_rom_abs}" _rom_seen)
        if(NOT _rom_seen EQUAL -1)
            continue()
        endif()
        list(APPEND _gba_verify_paths "${_rom_abs}")
        get_filename_component(_rom_name "${_rom}" NAME_WE)
        string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _rom_slug "${_rom_name}")
        set(_test_name "rom_${_rom_slug}_screenshots")
        if(_test_name IN_LIST _gba_verify_names)
            message(FATAL_ERROR "ROM verification test name collision for '${_rom}'")
        endif()
        list(APPEND _gba_verify_names "${_test_name}")
        set(_verify_args --frames "${GBA_VERIFY_FRAMES}" --min-colors "${GBA_VERIFY_MIN_COLORS}")
        if(GBA_ZELDA_ROM AND _rom_abs STREQUAL _zelda_rom_abs)
            list(APPEND _verify_args --sha256 "bedc74df62755f705398273de8ed3bc59be610cf55760d0b9aa277f1f5035e73" --min-colors 100)
        endif()
        add_test(NAME "${_test_name}" COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_rom.py"
            --emulator "$<TARGET_FILE:gba_emu>" --reference "$<TARGET_FILE:mgba_frame_capture>"
            --rom "${_rom_abs}" --output-dir "${CMAKE_CURRENT_BINARY_DIR}/rom-captures/${_rom_slug}"
            ${_verify_args})
        set_tests_properties("${_test_name}" PROPERTIES LABELS "local-rom" TIMEOUT 900)
    endforeach()
endif()
