#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdio>
#include <utility>
#include <vector>
#include "memory/bus.hpp"
#include "cpu/cpu.hpp"

namespace {

    void Put32(std::vector<u8>& rom, std::size_t offset, u32 value) {
        rom[offset + 0] = static_cast<u8>(value & 0xFF);
        rom[offset + 1] = static_cast<u8>((value >> 8) & 0xFF);
        rom[offset + 2] = static_cast<u8>((value >> 16) & 0xFF);
        rom[offset + 3] = static_cast<u8>((value >> 24) & 0xFF);
    }

    u32 Encode(u32 cond, u32 opcode, bool s, u32 rn, u32 rd, u32 rm) {
        return (cond << 28) | (0b00 << 26) | (opcode << 21) | ((s ? 1u : 0u) << 20) | (rn << 16) | (rd << 12) | rm;
    }

    u32 EncodeShiftedRegister(u32 cond, u32 opcode, bool s, u32 rn, u32 rd,
                              u32 shift_amount, u32 shift_type, u32 rm) {
        return (cond << 28) | (opcode << 21) | ((s ? 1u : 0u) << 20) |
               (rn << 16) | (rd << 12) | (shift_amount << 7) |
               (shift_type << 5) | rm;
    }

    std::vector<u8> AsRom(u32 instr) {
        return {static_cast<u8>(instr), static_cast<u8>(instr >> 8),
                static_cast<u8>(instr >> 16), static_cast<u8>(instr >> 24)};
    }

}  // namespace

TEST_CASE("Cpu::CheckCondition") {
    struct TestCase { u32 cond; u32 cpsr; bool expected; const char* name; };
    TestCase tests[] = {
        {0x0, 0x40000000, true,  "EQ with Z=1"},
        {0x0, 0x00000000, false, "EQ with Z=0"},
        {0xA, 0x00000000, true,  "GE with N=0,V=0"},
        {0xA, 0x80000000, false, "GE with N=1,V=0"},
        {0xE, 0x00000000, true,  "AL always true"},
    };
    for (auto& t : tests) {
        bool result = Cpu::CheckCondition(t.cond, t.cpsr);
        std::printf("%-16s expected=%d got=%d %s\n",
            t.name, t.expected, result, result == t.expected ? "OK" : "MISMATCH");
        REQUIRE(result == t.expected);
    }
}

TEST_CASE("Cpu::Step ADD sets r0 and clears flags") {
    Bus bus(AsRom(Encode(0xE, 0x4, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 5);
    cpu.SetRegister(2, 3);
    cpu.Step();
    std::printf("ADD: r0=%u (expect 8)\n", cpu.GetRegister(0));
    REQUIRE(cpu.GetRegister(0) == 8);
    REQUIRE(((cpu.GetCpsr() >> 30) & 1) == 0);
    REQUIRE(((cpu.GetCpsr() >> 29) & 1) == 0);
}

TEST_CASE("Cpu::Step CMP leaves r1 unchanged and sets flags") {
    Bus bus(AsRom(Encode(0xE, 0xA, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 5);
    cpu.SetRegister(2, 3);
    cpu.Step();
    std::printf("CMP: r1=%u (expect 5, unchanged), Z=%u (expect 0)\n",
        cpu.GetRegister(1), (cpu.GetCpsr() >> 30) & 1);
    REQUIRE(cpu.GetRegister(1) == 5);
    REQUIRE(((cpu.GetCpsr() >> 30) & 1) == 0);
}

TEST_CASE("Cpu::Step executes sequential instructions") {
    u32 i1 = Encode(0xE, 0xD, false, 0, 0, 5);   // MOV r0, r5
    u32 i2 = Encode(0xE, 0x4, false, 0, 3, 0);   // ADD r3, r0, r0
    std::vector<u8> rom;
    for (u32 w : {i1, i2}) {
        rom.push_back(w & 0xFF); rom.push_back((w >> 8) & 0xFF);
        rom.push_back((w >> 16) & 0xFF); rom.push_back((w >> 24) & 0xFF);
    }

    Bus bus(rom);
    Cpu cpu(bus);
    cpu.SetRegister(5, 10);

    cpu.Step();
    cpu.Step();

    std::printf("r0=%u (expect 10), r3=%u (expect 20)\n", cpu.GetRegister(0), cpu.GetRegister(3));
    REQUIRE(cpu.GetRegister(0) == 10);
    REQUIRE(cpu.GetRegister(3) == 20);
}

TEST_CASE("Cpu::Step applies LSL to a register operand") {
    Bus bus(AsRom(EncodeShiftedRegister(0xE, 0xD, false, 0, 0, 2, 0, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 3);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 12);
}

TEST_CASE("Cpu::Step applies LSR to a register operand") {
    Bus bus(AsRom(EncodeShiftedRegister(0xE, 0xD, false, 0, 0, 2, 1, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 16);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 4);
}

TEST_CASE("Cpu::Step applies ASR to a register operand") {
    Bus bus(AsRom(EncodeShiftedRegister(0xE, 0xD, false, 0, 0, 2, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x80000008);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xE0000002);
}

TEST_CASE("Cpu::Step applies ROR to a register operand") {
    Bus bus(AsRom(EncodeShiftedRegister(0xE, 0xD, false, 0, 0, 4, 3, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x80000001);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x18000000);
}

TEST_CASE("BX switches to Thumb when bit zero is set") {
    std::vector<u8> rom(0x20, 0);
    Put32(rom, 0, 0xE12FFF10); // BX r0

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x08000005);

    cpu.Step();

    REQUIRE((cpu.GetCpsr() & kThumbFlag) != 0u);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("BX switches to ARM when bit zero is clear") {
    std::vector<u8> rom(0x20, 0);
    Put32(rom, 0, 0xE12FFF10); // BX r0

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x08000008);

    cpu.Step();

    REQUIRE((cpu.GetCpsr() & kThumbFlag) == 0u);
    REQUIRE(cpu.GetRegister(15) == 0x08000008u);
}

TEST_CASE("BX uses the Rm target register") {
    std::vector<u8> rom(0x20, 0);
    Put32(rom, 0, 0xE12FFF13); // BX r3

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x08000010);
    cpu.SetRegister(3, 0x08000005);

    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
    REQUIRE((cpu.GetCpsr() & kThumbFlag) != 0u);
}

TEST_CASE("BX does not modify the link register") {
    std::vector<u8> rom(0x20, 0);
    Put32(rom, 0, 0xE12FFF10); // BX r0

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x08000005);
    cpu.SetRegister(14, 0x12345678);

    cpu.Step();

    REQUIRE(cpu.GetRegister(14) == 0x12345678u);
}

TEST_CASE("BX makes the next fetch use Thumb width") {
    std::vector<u8> rom(0x20, 0);
    Put32(rom, 0, 0xE12FFF10); // BX r0

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x08000005);

    cpu.Step();
    REQUIRE((cpu.GetCpsr() & kThumbFlag) != 0u);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);

    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x08000006u);
}
