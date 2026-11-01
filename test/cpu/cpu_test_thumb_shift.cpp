#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "cpu_test_helpers.hpp"

namespace {

constexpr u32 kNegativeFlag = 1u << 31;
constexpr u32 kZeroFlag = 1u << 30;
constexpr u32 kCarryFlag = 1u << 29;
constexpr u32 kOverflowFlag = 1u << 28;
constexpr u32 kSystemMode = 0x1F;

u32 EncodeThumbShift(u32 operation, u32 amount, u32 rs, u32 rd) {
    return (operation << 11) | ((amount & 0x1F) << 6) |
           ((rs & 0x7) << 3) | (rd & 0x7);
}

u32 EncodeMsrRegister(u32 cond, u32 field_mask, u32 rm) {
    return (cond << 28) | 0x0120F000u | ((field_mask & 0xF) << 16) | rm;
}

void EnterThumbAtRomStart(Cpu& cpu, u32 initial_cpsr = kSystemMode) {
    cpu.SetRegister(0, initial_cpsr);
    cpu.ExecuteMsr(EncodeMsrRegister(0xE, 0x9, 0), false);

    cpu.SetRegister(0, 0x08000001u);
    cpu.ExecuteBranchExchange(0xE12FFF10u);
    cpu.SetRegister(15, 0x08000000u);
}

void RequireFlags(Cpu& cpu, bool n, bool z, bool c, bool v) {
    u32 cpsr = cpu.GetCpsr();
    REQUIRE(((cpsr & kNegativeFlag) != 0u) == n);
    REQUIRE(((cpsr & kZeroFlag) != 0u) == z);
    REQUIRE(((cpsr & kCarryFlag) != 0u) == c);
    REQUIRE(((cpsr & kOverflowFlag) != 0u) == v);
}

}  // namespace

TEST_CASE("Thumb LSL shifts a register left") {
    Bus bus(cpu_test::AsRom(0x0048u));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu);
    cpu.SetRegister(1, 3);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 6u);
    RequireFlags(cpu, false, false, false, false);
}

TEST_CASE("Thumb LSL shifts a one into carry") {
    Bus bus(cpu_test::AsRom(0x0048u));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu);
    cpu.SetRegister(1, 0x80000001u);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x00000002u);
    RequireFlags(cpu, false, false, true, false);
}

TEST_CASE("Thumb LSL by zero preserves carry") {
    for (u32 initial_carry : {0u, kCarryFlag}) {
        Bus bus(cpu_test::AsRom(
            static_cast<u32>(EncodeThumbShift(0, 0, 1, 0))));
        Cpu cpu(bus);
        EnterThumbAtRomStart(cpu, kSystemMode | initial_carry);
        cpu.SetRegister(1, 0x12345678u);

        cpu.Step();

        REQUIRE(cpu.GetRegister(0) == 0x12345678u);
        RequireFlags(cpu, false, false, initial_carry != 0u, false);
    }
}

TEST_CASE("Thumb LSL produces zero and carries out bit 1") {
    Bus bus(cpu_test::AsRom(
        static_cast<u32>(EncodeThumbShift(0, 31, 1, 0))));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu);
    cpu.SetRegister(1, 2);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0u);
    RequireFlags(cpu, false, true, true, false);
}

TEST_CASE("Thumb LSR shifts a register right") {
    Bus bus(cpu_test::AsRom(0x0848u));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu);
    cpu.SetRegister(1, 0x80000002u);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x40000001u);
    RequireFlags(cpu, false, false, false, false);
}

TEST_CASE("Thumb LSR shifts a one into carry") {
    Bus bus(cpu_test::AsRom(0x0848u));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu);
    cpu.SetRegister(1, 3);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 1u);
    RequireFlags(cpu, false, false, true, false);
}

TEST_CASE("Thumb LSR encoded zero means a shift of 32") {
    Bus bus(cpu_test::AsRom(0x0808u));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu);
    cpu.SetRegister(1, 0x80000001u);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0u);
    RequireFlags(cpu, false, true, true, false);
}

TEST_CASE("Thumb ASR preserves the sign of a positive value") {
    Bus bus(cpu_test::AsRom(0x1048u));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu);
    cpu.SetRegister(1, 0x40000000u);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x20000000u);
    RequireFlags(cpu, false, false, false, false);
}

TEST_CASE("Thumb ASR preserves the sign of a negative value") {
    Bus bus(cpu_test::AsRom(0x1048u));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu);
    cpu.SetRegister(1, 0x80000000u);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xC0000000u);
    RequireFlags(cpu, true, false, false, false);
}

TEST_CASE("Thumb ASR carries out a set low bit from a negative value") {
    Bus bus(cpu_test::AsRom(0x1048u));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu);
    cpu.SetRegister(1, 0x80000001u);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xC0000000u);
    RequireFlags(cpu, true, false, true, false);
}

TEST_CASE("Thumb ASR by 32 produces all ones for a negative value") {
    Bus bus(cpu_test::AsRom(0x1008u));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu);
    cpu.SetRegister(1, 0x80000000u);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xFFFFFFFFu);
    RequireFlags(cpu, true, false, true, false);
}

TEST_CASE("Thumb ASR by 32 produces zero for a positive value") {
    Bus bus(cpu_test::AsRom(0x1008u));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu);
    cpu.SetRegister(1, 0x7FFFFFFFu);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0u);
    RequireFlags(cpu, false, true, false, false);
}

TEST_CASE("Thumb shifts preserve the V flag") {
    Bus bus(cpu_test::AsRom(0x0048u));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu, kSystemMode | kOverflowFlag);
    cpu.SetRegister(1, 0x80000001u);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x00000002u);
    RequireFlags(cpu, false, false, true, true);
}

TEST_CASE("Thumb LSL permits the same source and destination register") {
    Bus bus(cpu_test::AsRom(
        static_cast<u32>(EncodeThumbShift(0, 3, 1, 1))));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu);
    cpu.SetRegister(1, 5);

    cpu.Step();

    REQUIRE(cpu.GetRegister(1) == 40u);
}

TEST_CASE("Thumb shifts advance the PC by two bytes") {
    Bus bus(cpu_test::AsRom(0x0048u));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu);
    cpu.SetRegister(1, 3);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 6u);
    REQUIRE(cpu.GetRegister(15) == 0x08000002u);
}

TEST_CASE("Thumb add/subtract encoding is not consumed as a shift") {
    Bus bus(cpu_test::AsRom(0x1808u));
    Cpu cpu(bus);
    EnterThumbAtRomStart(cpu);
    cpu.SetRegister(0, 0xDEADBEEFu);
    cpu.SetRegister(1, 3);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) != 6u);
    REQUIRE(cpu.GetRegister(15) == 0x08000002u);
}
