#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "cpu_test_helpers.hpp"

TEST_CASE("Cpu::Step executes sequential instructions") {
    u32 i1 = cpu_test::Encode(0xE, 0xD, false, 0, 0, 5);   // MOV r0, r5
    u32 i2 = cpu_test::Encode(0xE, 0x4, false, 0, 3, 0);   // ADD r3, r0, r0
    std::vector<u8> rom;
    for (u32 w : {i1, i2}) {
        rom.push_back(w & 0xFF);
        rom.push_back((w >> 8) & 0xFF);
        rom.push_back((w >> 16) & 0xFF);
        rom.push_back((w >> 24) & 0xFF);
    }

    Bus bus(rom);
    Cpu cpu(bus);
    cpu.SetRegister(5, 10);

    cpu.Step();
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 10);
    REQUIRE(cpu.GetRegister(3) == 20);
}
