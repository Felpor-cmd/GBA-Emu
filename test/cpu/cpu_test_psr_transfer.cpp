#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "cpu_test_helpers.hpp"

namespace {

constexpr u32 kControlField = 0x1;
constexpr u32 kFlagsField = 0x8;

u32 EncodeMrs(u32 cond, bool spsr, u32 rd) {
    return (cond << 28) | 0x010F0000u | (spsr ? 1u << 22 : 0) |
           (rd << 12);
}

u32 EncodeMsrRegister(u32 cond, bool spsr, u32 field_mask, u32 rm) {
    return (cond << 28) | 0x0120F000u | (spsr ? 1u << 22 : 0) |
           ((field_mask & 0xF) << 16) | rm;
}

u32 EncodeMsrImmediate(u32 cond, bool spsr, u32 field_mask, u32 rotate,
                       u32 immediate) {
    return (cond << 28) | 0x0320F000u | (spsr ? 1u << 22 : 0) |
           ((field_mask & 0xF) << 16) | ((rotate & 0xF) << 8) |
           (immediate & 0xFF);
}

}  // namespace

TEST_CASE("Cpu::Step reads the CPSR with MRS") {
    std::vector<u8> rom(12, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0xE, false, kFlagsField, 2));
    cpu_test::Put32(rom, 8, EncodeMrs(0xE, false, 0));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x00000013u);
    cpu.SetRegister(2, 0xA0000000u);

    cpu.Step();
    cpu.Step();
    u32 cpsr_before_mrs = cpu.GetCpsr();
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xA0000013u);
    REQUIRE(cpu.GetCpsr() == cpsr_before_mrs);
    REQUIRE(cpu.GetRegister(15) == 0x0800000Cu);
}

TEST_CASE("Cpu::Step writes CPSR flags from a register") {
    Bus bus(cpu_test::AsRom(
        EncodeMsrRegister(0xE, false, kFlagsField, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xB0000000u);

    cpu.Step();

    REQUIRE(cpu.GetCpsr() == 0xB000001Fu);
    REQUIRE(cpu.GetRegister(0) == 0xB0000000u);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("Cpu::Step preserves CPSR control during a flags-only MSR") {
    std::vector<u8> rom(8, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0xE, false, kFlagsField, 0));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x60000000u);
    cpu.SetRegister(1, 0x000000D3u);

    cpu.Step();
    REQUIRE((cpu.GetCpsr() & 0xFFu) == 0xD3u);
    cpu.Step();

    REQUIRE(cpu.GetCpsr() == 0x600000D3u);
}

TEST_CASE("Cpu::Step preserves CPSR flags during a control-only MSR") {
    std::vector<u8> rom(8, 0);
    // ADDS r4, r5, r6 establishes N=1, Z=0, C=1, V=0.
    cpu_test::Put32(rom, 0, cpu_test::Encode(0xE, 0x4, true, 5, 4, 6));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0xE, false, kControlField, 0));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x0000009Fu);
    cpu.SetRegister(5, 0xFFFFFFFFu);
    cpu.SetRegister(6, 0x80000001u);

    cpu.Step();
    REQUIRE((cpu.GetCpsr() & 0xF0000000u) == 0xA0000000u);
    cpu.Step();

    REQUIRE(cpu.GetCpsr() == 0xA000009Fu);
}

TEST_CASE("Cpu::Step writes rotated immediate CPSR flags") {
    // Immediate 2 rotated right by 2 produces 0x80000000.
    Bus bus(cpu_test::AsRom(
        EncodeMsrImmediate(0xE, false, kFlagsField, 1, 2)));
    Cpu cpu(bus);

    cpu.Step();

    REQUIRE(cpu.GetCpsr() == 0x8000001Fu);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("Cpu::Step prevents User mode from writing CPSR control") {
    std::vector<u8> rom(8, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0xE, false, kControlField, 0));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x000000D2u);
    cpu.SetRegister(1, 0x00000010u);

    cpu.Step();
    REQUIRE((cpu.GetCpsr() & 0xFFu) == 0x10u);
    cpu.Step();

    REQUIRE((cpu.GetCpsr() & 0xFFu) == 0x10u);
    REQUIRE(cpu.GetRegister(15) == 0x08000008u);
}

TEST_CASE("Cpu::Step allows User mode to write CPSR flags") {
    std::vector<u8> rom(8, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0xE, false, kFlagsField, 0));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xF0000000u);
    cpu.SetRegister(1, 0x00000010u);

    cpu.Step();
    cpu.Step();

    REQUIRE(cpu.GetCpsr() == 0xF0000010u);
}

TEST_CASE("Cpu::Step reads the current mode SPSR") {
    std::vector<u8> rom(16, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(
        0xE, true, kFlagsField | kControlField, 0));
    cpu_test::Put32(rom, 8, EncodeMrs(0xE, true, 2));
    cpu_test::Put32(rom, 12, EncodeMrs(0xE, true, 3));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x60000010u);
    cpu.SetRegister(1, 0x00000013u);

    cpu.Step();
    cpu.Step();
    u32 cpsr_before_mrs = cpu.GetCpsr();
    cpu.Step();
    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x60000010u);
    REQUIRE(cpu.GetRegister(3) == 0x60000010u);
    REQUIRE(cpu.GetCpsr() == cpsr_before_mrs);
}

TEST_CASE("Cpu::Step keeps SPSRs independently banked") {
    std::vector<u8> rom(28, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 10));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(
        0xE, true, kFlagsField | kControlField, 1));
    cpu_test::Put32(rom, 8, EncodeMsrRegister(0xE, false, kControlField, 11));
    cpu_test::Put32(rom, 12, EncodeMsrRegister(
        0xE, true, kFlagsField | kControlField, 2));
    cpu_test::Put32(rom, 16, EncodeMrs(0xE, true, 3));
    cpu_test::Put32(rom, 20, EncodeMsrRegister(0xE, false, kControlField, 10));
    cpu_test::Put32(rom, 24, EncodeMrs(0xE, true, 4));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x10000011u);
    cpu.SetRegister(2, 0x20000012u);
    cpu.SetRegister(10, 0x00000013u);
    cpu.SetRegister(11, 0x00000012u);

    cpu.Step();
    cpu.Step();
    cpu.Step();
    cpu.Step();
    cpu.Step();
    cpu.Step();
    cpu.Step();

    REQUIRE(cpu.GetRegister(3) == 0x20000012u);
    REQUIRE(cpu.GetRegister(4) == 0x10000011u);
}

TEST_CASE("Cpu::Step writes flags to the current IRQ SPSR") {
    std::vector<u8> rom(16, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0xE, true, kControlField, 2));
    cpu_test::Put32(rom, 8, EncodeMsrRegister(0xE, true, kFlagsField, 0));
    cpu_test::Put32(rom, 12, EncodeMrs(0xE, true, 3));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xC0000000u);
    cpu.SetRegister(1, 0x00000012u);
    cpu.SetRegister(2, 0x000000D3u);

    cpu.Step();
    u32 cpsr_before_spsr_writes = cpu.GetCpsr();
    cpu.Step();
    cpu.Step();
    cpu.Step();

    REQUIRE(cpu.GetRegister(3) == 0xC00000D3u);
    REQUIRE(cpu.GetCpsr() == cpsr_before_spsr_writes);
}

TEST_CASE("Cpu::Step writes control to the current Supervisor SPSR") {
    std::vector<u8> rom(16, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0xE, true, kFlagsField, 2));
    cpu_test::Put32(rom, 8, EncodeMsrRegister(0xE, true, kControlField, 0));
    cpu_test::Put32(rom, 12, EncodeMrs(0xE, true, 3));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x000000D2u);
    cpu.SetRegister(1, 0x00000013u);
    cpu.SetRegister(2, 0xA0000000u);

    cpu.Step();
    u32 cpsr_before_spsr_writes = cpu.GetCpsr();
    cpu.Step();
    cpu.Step();
    cpu.Step();

    REQUIRE(cpu.GetRegister(3) == 0xA00000D2u);
    REQUIRE(cpu.GetCpsr() == cpsr_before_spsr_writes);
    REQUIRE((cpu.GetCpsr() & 0x1Fu) == 0x13u);
}

TEST_CASE("Cpu::Step treats MRS SPSR in System mode as unsupported") {
    Bus bus(cpu_test::AsRom(EncodeMrs(0xE, true, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xDEADBEEFu);
    u32 initial_cpsr = cpu.GetCpsr();

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEFu);
    REQUIRE(cpu.GetCpsr() == initial_cpsr);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("Cpu::Step treats MSR SPSR in System mode as unsupported") {
    std::vector<u8> rom(24, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(
        0xE, true, kFlagsField | kControlField, 2));
    cpu_test::Put32(rom, 8, EncodeMsrRegister(0xE, false, kControlField, 3));
    cpu_test::Put32(rom, 12, EncodeMsrRegister(0xE, true, kFlagsField, 0));
    cpu_test::Put32(rom, 16, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 20, EncodeMrs(0xE, true, 4));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xF0000000u);
    cpu.SetRegister(1, 0x00000013u);
    cpu.SetRegister(2, 0x60000010u);
    cpu.SetRegister(3, 0x0000001Fu);

    cpu.Step();
    cpu.Step();
    cpu.Step();
    cpu.Step();
    cpu.Step();
    cpu.Step();

    REQUIRE(cpu.GetRegister(4) == 0x60000010u);
}

TEST_CASE("Cpu::Step prevents MSR from entering Thumb state") {
    Bus bus(cpu_test::AsRom(
        EncodeMsrRegister(0xE, false, kControlField, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x0000003Fu);

    cpu.Step();

    REQUIRE((cpu.GetCpsr() & kThumbFlag) == 0);
    REQUIRE((cpu.GetCpsr() & 0x1Fu) == 0x1Fu);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("Cpu::Step preserves reserved CPSR bits during MSR") {
    Bus bus(cpu_test::AsRom(
        EncodeMsrRegister(0xE, false, 0xF, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xFFFFFFFFu);

    cpu.Step();

    REQUIRE(cpu.GetCpsr() == 0xF00000DFu);
}

TEST_CASE("Cpu::Step suppresses a condition-failed MRS") {
    Bus bus(cpu_test::AsRom(EncodeMrs(0x0, false, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xDEADBEEFu);
    u32 initial_cpsr = cpu.GetCpsr();

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEFu);
    REQUIRE(cpu.GetCpsr() == initial_cpsr);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("Cpu::Step suppresses a condition-failed MSR") {
    std::vector<u8> rom(8, 0);
    // MOVS r4, r4 establishes Z=1, making the following MSRNE fail.
    cpu_test::Put32(rom, 0, cpu_test::Encode(0xE, 0xD, true, 0, 4, 4));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0x1, false, kFlagsField, 0));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xF0000000u);
    cpu.SetRegister(4, 0);

    cpu.Step();
    u32 cpsr_before_msr = cpu.GetCpsr();
    cpu.Step();

    REQUIRE(cpu.GetCpsr() == cpsr_before_msr);
    REQUIRE(cpu.GetRegister(15) == 0x08000008u);
}

TEST_CASE("Cpu::Step gives PSR transfers decode precedence") {
    std::vector<u8> rom(8, 0);
    cpu_test::Put32(rom, 0, 0xE10F1000u);  // MRS r1, CPSR
    cpu_test::Put32(rom, 4, 0xE128F002u);  // MSR CPSR_f, r2

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0xDEADBEEFu);
    cpu.SetRegister(2, 0xC0000000u);

    cpu.Step();
    REQUIRE(cpu.GetRegister(1) == 0x0000001Fu);
    cpu.Step();

    REQUIRE(cpu.GetCpsr() == 0xC000001Fu);
}

TEST_CASE("Cpu::Step reads CPSR while in User mode") {
    std::vector<u8> rom(8, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 4, EncodeMrs(0xE, false, 2));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x00000010u);

    cpu.Step();
    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x00000010u);
}

TEST_CASE("Cpu::Step writes rotated immediate flags to the current SPSR") {
    std::vector<u8> rom(16, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0xE, true, kControlField, 2));
    cpu_test::Put32(rom, 8, EncodeMsrImmediate(0xE, true, kFlagsField, 1, 2));
    cpu_test::Put32(rom, 12, EncodeMrs(0xE, true, 3));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x00000012u);
    cpu.SetRegister(2, 0x00000012u);

    cpu.Step();
    cpu.Step();
    cpu.Step();
    cpu.Step();

    REQUIRE(cpu.GetRegister(3) == 0x80000012u);
    REQUIRE(cpu.GetCpsr() == 0x00000012u);
}

TEST_CASE("Cpu::Step keeps User and System registers shared") {
    Bus bus(cpu_test::AsRom(
        EncodeMsrRegister(0xE, false, kControlField, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x00000010u);
    cpu.SetRegister(13, 0x11111111u);
    cpu.SetRegister(14, 0x22222222u);

    cpu.Step();

    REQUIRE((cpu.GetCpsr() & 0x1Fu) == 0x10u);
    REQUIRE(cpu.GetRegister(13) == 0x11111111u);
    REQUIRE(cpu.GetRegister(14) == 0x22222222u);
}

TEST_CASE("Cpu::Step banks exception mode stack and link registers") {
    std::vector<u8> rom(16, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 0));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 8, EncodeMsrRegister(0xE, false, kControlField, 0));
    cpu_test::Put32(rom, 12, EncodeMsrRegister(0xE, false, kControlField, 2));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x00000013u);
    cpu.SetRegister(1, 0x00000012u);
    cpu.SetRegister(2, 0x0000001Fu);
    cpu.SetRegister(13, 0x11111111u);
    cpu.SetRegister(14, 0x11111112u);

    cpu.Step();
    REQUIRE(cpu.GetRegister(13) == 0);
    REQUIRE(cpu.GetRegister(14) == 0);
    cpu.SetRegister(13, 0x22222221u);
    cpu.SetRegister(14, 0x22222222u);

    cpu.Step();
    REQUIRE(cpu.GetRegister(13) == 0);
    REQUIRE(cpu.GetRegister(14) == 0);
    cpu.SetRegister(13, 0x33333331u);
    cpu.SetRegister(14, 0x33333332u);

    cpu.Step();
    REQUIRE(cpu.GetRegister(13) == 0x22222221u);
    REQUIRE(cpu.GetRegister(14) == 0x22222222u);

    cpu.Step();
    REQUIRE(cpu.GetRegister(13) == 0x11111111u);
    REQUIRE(cpu.GetRegister(14) == 0x11111112u);
}

TEST_CASE("Cpu::Step banks Abort and Undefined stack and link registers") {
    std::vector<u8> rom(16, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 0));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 8, EncodeMsrRegister(0xE, false, kControlField, 0));
    cpu_test::Put32(rom, 12, EncodeMsrRegister(0xE, false, kControlField, 2));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x00000017u);
    cpu.SetRegister(1, 0x0000001Bu);
    cpu.SetRegister(2, 0x0000001Fu);
    cpu.SetRegister(13, 0x11111111u);
    cpu.SetRegister(14, 0x11111112u);

    cpu.Step();
    REQUIRE(cpu.GetRegister(13) == 0);
    REQUIRE(cpu.GetRegister(14) == 0);
    cpu.SetRegister(13, 0x22222221u);
    cpu.SetRegister(14, 0x22222222u);

    cpu.Step();
    REQUIRE(cpu.GetRegister(13) == 0);
    REQUIRE(cpu.GetRegister(14) == 0);
    cpu.SetRegister(13, 0x33333331u);
    cpu.SetRegister(14, 0x33333332u);

    cpu.Step();
    REQUIRE(cpu.GetRegister(13) == 0x22222221u);
    REQUIRE(cpu.GetRegister(14) == 0x22222222u);

    cpu.Step();
    REQUIRE(cpu.GetRegister(13) == 0x11111111u);
    REQUIRE(cpu.GetRegister(14) == 0x11111112u);
}

TEST_CASE("Cpu::Step banks FIQ registers r8 through r14") {
    std::vector<u8> rom(12, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 0));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 8, EncodeMsrRegister(0xE, false, kControlField, 0));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x00000011u);
    cpu.SetRegister(1, 0x0000001Fu);
    cpu.SetRegister(8, 0x10000008u);
    cpu.SetRegister(9, 0x10000009u);
    cpu.SetRegister(10, 0x1000000Au);
    cpu.SetRegister(11, 0x1000000Bu);
    cpu.SetRegister(12, 0x1000000Cu);
    cpu.SetRegister(13, 0x1000000Du);
    cpu.SetRegister(14, 0x1000000Eu);

    cpu.Step();
    REQUIRE(cpu.GetRegister(8) == 0);
    REQUIRE(cpu.GetRegister(9) == 0);
    REQUIRE(cpu.GetRegister(10) == 0);
    REQUIRE(cpu.GetRegister(11) == 0);
    REQUIRE(cpu.GetRegister(12) == 0);
    REQUIRE(cpu.GetRegister(13) == 0);
    REQUIRE(cpu.GetRegister(14) == 0);
    cpu.SetRegister(8, 0x20000008u);
    cpu.SetRegister(9, 0x20000009u);
    cpu.SetRegister(10, 0x2000000Au);
    cpu.SetRegister(11, 0x2000000Bu);
    cpu.SetRegister(12, 0x2000000Cu);
    cpu.SetRegister(13, 0x2000000Du);
    cpu.SetRegister(14, 0x2000000Eu);

    cpu.Step();
    REQUIRE(cpu.GetRegister(8) == 0x10000008u);
    REQUIRE(cpu.GetRegister(9) == 0x10000009u);
    REQUIRE(cpu.GetRegister(10) == 0x1000000Au);
    REQUIRE(cpu.GetRegister(11) == 0x1000000Bu);
    REQUIRE(cpu.GetRegister(12) == 0x1000000Cu);
    REQUIRE(cpu.GetRegister(13) == 0x1000000Du);
    REQUIRE(cpu.GetRegister(14) == 0x1000000Eu);

    cpu.Step();
    REQUIRE(cpu.GetRegister(8) == 0x20000008u);
    REQUIRE(cpu.GetRegister(9) == 0x20000009u);
    REQUIRE(cpu.GetRegister(10) == 0x2000000Au);
    REQUIRE(cpu.GetRegister(11) == 0x2000000Bu);
    REQUIRE(cpu.GetRegister(12) == 0x2000000Cu);
    REQUIRE(cpu.GetRegister(13) == 0x2000000Du);
    REQUIRE(cpu.GetRegister(14) == 0x2000000Eu);
}

TEST_CASE("Cpu::Step keeps FIQ Abort and Undefined SPSRs independent") {
    std::vector<u8> rom(48, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 0));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(
        0xE, true, kFlagsField | kControlField, 1));
    cpu_test::Put32(rom, 8, EncodeMsrRegister(0xE, false, kControlField, 0));
    cpu_test::Put32(rom, 12, EncodeMsrRegister(
        0xE, true, kFlagsField | kControlField, 1));
    cpu_test::Put32(rom, 16, EncodeMsrRegister(0xE, false, kControlField, 0));
    cpu_test::Put32(rom, 20, EncodeMsrRegister(
        0xE, true, kFlagsField | kControlField, 1));
    cpu_test::Put32(rom, 24, EncodeMsrRegister(0xE, false, kControlField, 0));
    cpu_test::Put32(rom, 28, EncodeMrs(0xE, true, 2));
    cpu_test::Put32(rom, 32, EncodeMsrRegister(0xE, false, kControlField, 0));
    cpu_test::Put32(rom, 36, EncodeMrs(0xE, true, 3));
    cpu_test::Put32(rom, 40, EncodeMsrRegister(0xE, false, kControlField, 0));
    cpu_test::Put32(rom, 44, EncodeMrs(0xE, true, 4));

    Bus bus(std::move(rom));
    Cpu cpu(bus);

    cpu.SetRegister(0, 0x00000011u);
    cpu.SetRegister(1, 0x10000010u);
    cpu.Step();
    cpu.Step();

    cpu.SetRegister(0, 0x00000017u);
    cpu.SetRegister(1, 0x40000013u);
    cpu.Step();
    cpu.Step();

    cpu.SetRegister(0, 0x0000001Bu);
    cpu.SetRegister(1, 0x80000017u);
    cpu.Step();
    cpu.Step();

    cpu.SetRegister(0, 0x00000011u);
    cpu.Step();
    cpu.Step();
    cpu.SetRegister(0, 0x00000017u);
    cpu.Step();
    cpu.Step();
    cpu.SetRegister(0, 0x0000001Bu);
    cpu.Step();
    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x10000010u);
    REQUIRE(cpu.GetRegister(3) == 0x40000013u);
    REQUIRE(cpu.GetRegister(4) == 0x80000017u);
}

TEST_CASE("Cpu::Step rejects an invalid CPSR mode") {
    Bus bus(cpu_test::AsRom(
        EncodeMsrRegister(0xE, false, kControlField, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0);
    u32 initial_cpsr = cpu.GetCpsr();
    u32 initial_sp = cpu.GetRegister(13);

    cpu.Step();

    REQUIRE(cpu.GetCpsr() == initial_cpsr);
    REQUIRE(cpu.GetRegister(13) == initial_sp);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("Cpu::Step treats MRS with r15 as unsupported") {
    Bus bus(cpu_test::AsRom(EncodeMrs(0xE, false, 15)));
    Cpu cpu(bus);
    u32 initial_cpsr = cpu.GetCpsr();

    cpu.Step();

    REQUIRE(cpu.GetCpsr() == initial_cpsr);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("Cpu::Step treats register MSR with r15 as unsupported") {
    std::vector<u8> rom(8, 0);
    // ADDS r4, r5, r6 establishes N=1 and V=1 before the invalid MSR.
    cpu_test::Put32(rom, 0, cpu_test::Encode(0xE, 0x4, true, 5, 4, 6));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0xE, false, kFlagsField, 15));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(5, 0x7FFFFFFFu);
    cpu.SetRegister(6, 1);

    cpu.Step();
    u32 cpsr_before_msr = cpu.GetCpsr();
    cpu.Step();

    REQUIRE(cpu.GetCpsr() == cpsr_before_msr);
    REQUIRE(cpu.GetRegister(15) == 0x08000008u);
}

TEST_CASE("Cpu::Step reads a banked MSR source before switching modes") {
    std::vector<u8> rom(8, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 13));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0xE, false, kControlField, 8));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(8, 0x00000011u);
    cpu.SetRegister(13, 0x00000013u);

    cpu.Step();
    REQUIRE((cpu.GetCpsr() & 0x1Fu) == 0x13u);
    REQUIRE(cpu.GetRegister(13) == 0);

    cpu.Step();
    REQUIRE((cpu.GetCpsr() & 0x1Fu) == 0x11u);
    REQUIRE(cpu.GetRegister(8) == 0);
}

TEST_CASE("Cpu::Reset clears every active and banked register view") {
    std::vector<u8> rom(12, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 0));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(0xE, false, kControlField, 1));
    cpu_test::Put32(rom, 8, EncodeMsrRegister(0xE, false, kControlField, 2));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x00000013u);
    cpu.SetRegister(1, 0x00000011u);
    cpu.SetRegister(2, 0x0000001Fu);
    cpu.SetRegister(8, 0x11111118u);
    cpu.SetRegister(13, 0x1111111Du);

    cpu.Step();
    cpu.SetRegister(8, 0x22222228u);
    cpu.SetRegister(13, 0x2222222Du);
    cpu.Step();
    cpu.SetRegister(8, 0x33333338u);
    cpu.SetRegister(13, 0x3333333Du);

    cpu.Reset();

    cpu.SetRegister(0, 0x00000013u);
    cpu.Step();
    REQUIRE(cpu.GetRegister(8) == 0);
    REQUIRE(cpu.GetRegister(13) == 0);

    cpu.SetRegister(1, 0x00000011u);
    cpu.Step();
    REQUIRE(cpu.GetRegister(8) == 0);
    REQUIRE(cpu.GetRegister(13) == 0);

    cpu.SetRegister(2, 0x0000001Fu);
    cpu.Step();
    REQUIRE(cpu.GetRegister(8) == 0);
    REQUIRE(cpu.GetRegister(13) == 0x03007F00u);
}
