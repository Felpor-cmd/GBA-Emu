#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "cpu_test_helpers.hpp"

TEST_CASE("ARM SWI enters Supervisor mode and returns with MOVS pc, lr") {
    std::vector<u8> bios(kBiosSize, 0);
    cpu_test::Put32(bios, 0x08, cpu_test::Encode(0xE, 0xD, true, 0, 15, 14));
    std::vector<u8> rom(8, 0);
    cpu_test::Put32(rom, 0, 0xEF123456u);
    cpu_test::Put32(rom, 4, cpu_test::Encode(0xE, 0xD, false, 0, 0, 1));
    Bus bus(std::move(rom), std::move(bios));
    Cpu cpu(bus);
    REQUIRE(cpu.SetCpsr(0xA000005Fu));

    cpu.Step();

    REQUIRE((cpu.GetCpsr() & 0x1Fu) == 0x13u);
    REQUIRE((cpu.GetCpsr() & 0x20u) == 0);
    REQUIRE((cpu.GetCpsr() & 0x80u) != 0);
    REQUIRE((cpu.GetCpsr() & 0x40u) != 0);
    REQUIRE(cpu.GetRegister(15) == 0x08u);
    REQUIRE(cpu.GetRegister(14) == 0x08000004u);
    cpu.Step();
    REQUIRE(cpu.GetCpsr() == 0xA000005Fu);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("Conditional ARM SWI has no exception side effects when condition fails") {
    Bus bus(cpu_test::AsRom(0x0F000000u));
    Cpu cpu(bus);
    const u32 cpsr_before = cpu.GetCpsr();

    cpu.Step();

    REQUIRE(cpu.GetCpsr() == cpsr_before);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}
