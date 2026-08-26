#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "cpu_test_helpers.hpp"

TEST_CASE("BX switches to Thumb when bit zero is set") {
    std::vector<u8> rom(0x20, 0);
    cpu_test::Put32(rom, 0, 0xE12FFF10); // BX r0

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x08000005);

    cpu.Step();

    REQUIRE((cpu.GetCpsr() & kThumbFlag) != 0u);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("BX switches to ARM when bit zero is clear") {
    std::vector<u8> rom(0x20, 0);
    cpu_test::Put32(rom, 0, 0xE12FFF10); // BX r0

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x08000008);

    cpu.Step();

    REQUIRE((cpu.GetCpsr() & kThumbFlag) == 0u);
    REQUIRE(cpu.GetRegister(15) == 0x08000008u);
}

TEST_CASE("BX uses the Rm target register") {
    std::vector<u8> rom(0x20, 0);
    cpu_test::Put32(rom, 0, 0xE12FFF13); // BX r3

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
    cpu_test::Put32(rom, 0, 0xE12FFF10); // BX r0

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x08000005);
    cpu.SetRegister(14, 0x12345678);

    cpu.Step();

    REQUIRE(cpu.GetRegister(14) == 0x12345678u);
}
