#include <catch2/catch_test_macros.hpp>

#include "cpu_test_helpers.hpp"

TEST_CASE("Cpu::Step ADD sets r0 and clears flags") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x4, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 5);
    cpu.SetRegister(2, 3);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 8);
    REQUIRE(((cpu.GetCpsr() >> 30) & 1) == 0);
    REQUIRE(((cpu.GetCpsr() >> 29) & 1) == 0);
}

TEST_CASE("Cpu::Step CMP leaves r1 unchanged and sets flags") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0xA, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 5);
    cpu.SetRegister(2, 3);
    cpu.Step();

    REQUIRE(cpu.GetRegister(1) == 5);
    REQUIRE(((cpu.GetCpsr() >> 30) & 1) == 0);
}
