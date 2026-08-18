#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <vector>
#include "cpu/cpu.hpp"

namespace {

    u32 Encode(u32 cond, u32 opcode, bool s, u32 rn, u32 rd, u32 rm) {
        return (cond << 28) | (0b00 << 26) | (opcode << 21) | ((s ? 1u : 0u) << 20) | (rn << 16) | (rd << 12) | rm;
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