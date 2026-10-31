#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "cpu_test_helpers.hpp"

TEST_CASE("BX makes the next fetch use Thumb width") {
    std::vector<u8> rom(0x20, 0);
    cpu_test::Put32(rom, 0, 0xE12FFF10); // BX r0

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x08000005);

    cpu.Step();
    REQUIRE((cpu.GetCpsr() & kThumbFlag) != 0u);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);

    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x08000006u);
}

TEST_CASE("Thumb BX makes the next fetch use ARM width") {
    std::vector<u8> rom(0x20, 0);
    cpu_test::Put32(rom, 0, 0xE12FFF10u);  // ARM BX r0
    rom[8] = 0x00;
    rom[9] = 0x47;  // Thumb BX r0
    cpu_test::Put32(rom, 16, 0xE3A02011u);  // MOV r2, #17
    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x08000009u);

    cpu.Step();  // ARM -> Thumb at 0x08000008
    cpu.SetRegister(0, 0x08000010u);
    cpu.Step();  // Thumb BX r0

    REQUIRE((cpu.GetCpsr() & kThumbFlag) == 0);
    REQUIRE(cpu.GetRegister(15) == 0x08000010u);
    cpu.Step();
    REQUIRE(cpu.GetRegister(2) == 17u);
}
