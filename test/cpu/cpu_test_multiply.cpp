#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "cpu_test_helpers.hpp"

namespace {

constexpr u32 kStatusFlags = 0xF0000000u;

u32 EncodeMultiply(u32 cond, bool accumulate, bool set_flags, u32 rd, u32 rn,
                   u32 rs, u32 rm) {
    return (cond << 28) | (accumulate ? 1u : 0u) << 21 |
           (set_flags ? 1u : 0u) << 20 | (rd << 16) | (rn << 12) |
           (rs << 8) | 0x90u | rm;
}

}  // namespace

TEST_CASE("Cpu::Step executes basic MUL") {
    Bus bus(cpu_test::AsRom(EncodeMultiply(0xE, false, false, 0, 0, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 6);
    cpu.SetRegister(2, 7);
    u32 initial_flags = cpu.GetCpsr() & kStatusFlags;

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 42);
    REQUIRE((cpu.GetCpsr() & kStatusFlags) == initial_flags);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("Cpu::Step leaves flags unchanged when MUL produces zero") {
    std::vector<u8> rom(8, 0);
    // ADDS r4, r5, r6 establishes N=1 and V=1 before MUL.
    cpu_test::Put32(rom, 0, cpu_test::Encode(0xE, 0x4, true, 5, 4, 6));
    cpu_test::Put32(rom, 4, EncodeMultiply(0xE, false, false, 0, 0, 2, 1));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xFFFFFFFFu);
    cpu.SetRegister(1, 0x12345678u);
    cpu.SetRegister(2, 0);
    cpu.SetRegister(5, 0x7FFFFFFFu);
    cpu.SetRegister(6, 1);

    cpu.Step();
    u32 cpsr_before_mul = cpu.GetCpsr();

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0);
    REQUIRE(cpu.GetCpsr() == cpsr_before_mul);
}

TEST_CASE("Cpu::Step sets MULS zero and clears negative") {
    std::vector<u8> rom(8, 0);
    // ADDS r4, r5, r6 establishes N=1 and V=1 before MULS.
    cpu_test::Put32(rom, 0, cpu_test::Encode(0xE, 0x4, true, 5, 4, 6));
    cpu_test::Put32(rom, 4, EncodeMultiply(0xE, false, true, 0, 0, 2, 1));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0);
    cpu.SetRegister(2, 100);
    cpu.SetRegister(5, 0x7FFFFFFFu);
    cpu.SetRegister(6, 1);

    cpu.Step();
    REQUIRE(((cpu.GetCpsr() >> 31) & 1u) == 1);
    REQUIRE(((cpu.GetCpsr() >> 28) & 1u) == 1);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0);
    REQUIRE(((cpu.GetCpsr() >> 31) & 1u) == 0);
    REQUIRE(((cpu.GetCpsr() >> 30) & 1u) == 1);
    REQUIRE(((cpu.GetCpsr() >> 28) & 1u) == 1);
}

TEST_CASE("Cpu::Step sets the MULS negative flag for a negative result") {
    Bus bus(cpu_test::AsRom(EncodeMultiply(0xE, false, true, 0, 0, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x80000000u);
    cpu.SetRegister(2, 1);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x80000000u);
    REQUIRE(((cpu.GetCpsr() >> 31) & 1u) == 1);
    REQUIRE(((cpu.GetCpsr() >> 30) & 1u) == 0);
}

TEST_CASE("Cpu::Step truncates MUL to the low 32 bits") {
    Bus bus(cpu_test::AsRom(EncodeMultiply(0xE, false, false, 0, 0, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0xFFFFFFFFu);
    cpu.SetRegister(2, 2);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xFFFFFFFEu);
}

TEST_CASE("Cpu::Step multiplies signed-looking operands as 32-bit values") {
    Bus bus(cpu_test::AsRom(EncodeMultiply(0xE, false, false, 0, 0, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0xFFFFFFF6u);
    cpu.SetRegister(2, 20);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xFFFFFF38u);
}

TEST_CASE("Cpu::Step executes basic MLA") {
    Bus bus(cpu_test::AsRom(EncodeMultiply(0xE, true, false, 0, 3, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 6);
    cpu.SetRegister(2, 7);
    cpu.SetRegister(3, 5);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 47);
}

TEST_CASE("Cpu::Step wraps MLA to the low 32 bits") {
    Bus bus(cpu_test::AsRom(EncodeMultiply(0xE, true, false, 0, 3, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0xFFFFFFFFu);
    cpu.SetRegister(2, 2);
    cpu.SetRegister(3, 3);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x00000001u);
}

TEST_CASE("Cpu::Step sets MLAS flags from the accumulated result") {
    std::vector<u8> rom(8, 0);
    // ADDS r4, r5, r6 establishes N=1 and V=1 before MLAS.
    cpu_test::Put32(rom, 0, cpu_test::Encode(0xE, 0x4, true, 5, 4, 6));
    cpu_test::Put32(rom, 4, EncodeMultiply(0xE, true, true, 0, 3, 2, 1));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0xFFFFFFFFu);
    cpu.SetRegister(2, 1);
    cpu.SetRegister(3, 1);
    cpu.SetRegister(5, 0x7FFFFFFFu);
    cpu.SetRegister(6, 1);

    cpu.Step();
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0);
    REQUIRE(((cpu.GetCpsr() >> 31) & 1u) == 0);
    REQUIRE(((cpu.GetCpsr() >> 30) & 1u) == 1);
    REQUIRE(((cpu.GetCpsr() >> 28) & 1u) == 1);
}

TEST_CASE("Cpu::Step reads the old accumulator when MLA writes it") {
    Bus bus(cpu_test::AsRom(EncodeMultiply(0xE, true, false, 3, 3, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 6);
    cpu.SetRegister(2, 7);
    cpu.SetRegister(3, 5);

    cpu.Step();

    REQUIRE(cpu.GetRegister(3) == 47);
}

TEST_CASE("Cpu::Step reads Rs before MUL writes the same register") {
    Bus bus(cpu_test::AsRom(EncodeMultiply(0xE, false, false, 2, 0, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 6);
    cpu.SetRegister(2, 7);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 42);
}

TEST_CASE("Cpu::Step treats MUL with nonzero Rn as unsupported") {
    Bus bus(cpu_test::AsRom(EncodeMultiply(0xE, false, false, 0, 3, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xDEADBEEFu);
    cpu.SetRegister(1, 3);
    cpu.SetRegister(2, 4);
    cpu.SetRegister(3, 100);
    u32 initial_cpsr = cpu.GetCpsr();

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEFu);
    REQUIRE(cpu.GetRegister(3) == 100);
    REQUIRE(cpu.GetCpsr() == initial_cpsr);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("Cpu::Step treats MUL with Rd equal to Rm as unsupported") {
    std::vector<u8> rom(8, 0);
    // ADDS r4, r5, r6 establishes N=1 and V=1 before the invalid MULS.
    cpu_test::Put32(rom, 0, cpu_test::Encode(0xE, 0x4, true, 5, 4, 6));
    cpu_test::Put32(rom, 4, EncodeMultiply(0xE, false, true, 1, 0, 2, 1));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xDEADBEEFu);
    cpu.SetRegister(1, 6);
    cpu.SetRegister(2, 7);
    cpu.SetRegister(5, 0x7FFFFFFFu);
    cpu.SetRegister(6, 1);

    cpu.Step();
    u32 cpsr_before_mul = cpu.GetCpsr();
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEFu);
    REQUIRE(cpu.GetRegister(1) == 6);
    REQUIRE(cpu.GetCpsr() == cpsr_before_mul);
    REQUIRE(cpu.GetRegister(15) == 0x08000008u);
}

TEST_CASE("Cpu::Step treats MUL with r15 as Rd as unsupported") {
    Bus bus(cpu_test::AsRom(EncodeMultiply(0xE, false, false, 15, 0, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 6);
    cpu.SetRegister(2, 7);
    u32 initial_cpsr = cpu.GetCpsr();

    cpu.Step();

    REQUIRE(cpu.GetRegister(1) == 6);
    REQUIRE(cpu.GetRegister(2) == 7);
    REQUIRE(cpu.GetCpsr() == initial_cpsr);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("Cpu::Step treats MUL with r15 as Rs as unsupported") {
    Bus bus(cpu_test::AsRom(EncodeMultiply(0xE, false, false, 0, 0, 15, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xDEADBEEFu);
    cpu.SetRegister(1, 6);
    u32 initial_cpsr = cpu.GetCpsr();

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEFu);
    REQUIRE(cpu.GetRegister(1) == 6);
    REQUIRE(cpu.GetCpsr() == initial_cpsr);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("Cpu::Step treats MUL with r15 as Rm as unsupported") {
    Bus bus(cpu_test::AsRom(EncodeMultiply(0xE, false, false, 0, 0, 2, 15)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xDEADBEEFu);
    cpu.SetRegister(2, 7);
    u32 initial_cpsr = cpu.GetCpsr();

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEFu);
    REQUIRE(cpu.GetRegister(2) == 7);
    REQUIRE(cpu.GetCpsr() == initial_cpsr);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("Cpu::Step treats MLA with r15 as Rn as unsupported") {
    Bus bus(cpu_test::AsRom(EncodeMultiply(0xE, true, false, 0, 15, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xDEADBEEFu);
    cpu.SetRegister(1, 6);
    cpu.SetRegister(2, 7);
    u32 initial_cpsr = cpu.GetCpsr();

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEFu);
    REQUIRE(cpu.GetRegister(1) == 6);
    REQUIRE(cpu.GetRegister(2) == 7);
    REQUIRE(cpu.GetCpsr() == initial_cpsr);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("Cpu::Step suppresses a condition-failed MUL") {
    std::vector<u8> rom(8, 0);
    // MOVS r4, r4 establishes Z=1, making the following MULNE fail.
    cpu_test::Put32(rom, 0, cpu_test::Encode(0xE, 0xD, true, 0, 4, 4));
    cpu_test::Put32(rom, 4, EncodeMultiply(0x1, false, false, 0, 0, 2, 1));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xDEADBEEFu);
    cpu.SetRegister(1, 6);
    cpu.SetRegister(2, 7);
    cpu.SetRegister(4, 0);

    cpu.Step();
    u32 flags_before_mul = cpu.GetCpsr();

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEFu);
    REQUIRE(cpu.GetCpsr() == flags_before_mul);
    REQUIRE(cpu.GetRegister(15) == 0x08000008u);
}

TEST_CASE("Cpu::Step executes MULEQ when its condition succeeds") {
    std::vector<u8> rom(8, 0);
    // MOVS r4, r4 establishes Z=1 before MULEQ.
    cpu_test::Put32(rom, 0, cpu_test::Encode(0xE, 0xD, true, 0, 4, 4));
    cpu_test::Put32(rom, 4, EncodeMultiply(0x0, false, false, 0, 0, 2, 1));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(1, 8);
    cpu.SetRegister(2, 9);
    cpu.SetRegister(4, 0);

    cpu.Step();
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 72);
    REQUIRE(cpu.GetRegister(15) == 0x08000008u);
}

TEST_CASE("Cpu::Step gives multiply encoding decode precedence") {
    // MUL r0, r1, r2, encoded directly in ARM instruction form.
    Bus bus(cpu_test::AsRom(0xE0000291u));
    Cpu cpu(bus);
    cpu.SetRegister(1, 3);
    cpu.SetRegister(2, 4);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 12);
}
