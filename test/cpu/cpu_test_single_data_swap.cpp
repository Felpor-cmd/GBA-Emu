#include <catch2/catch_test_macros.hpp>

#include "cpu_test_helpers.hpp"

TEST_CASE("ARM SWP swaps a word atomically") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataSwap(0xE, false, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    cpu.SetRegister(2, 0xDEADBEEF);
    bus.Write32(0x02000000, 0x12345678);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x12345678);
    REQUIRE(bus.Read32(0x02000000) == 0xDEADBEEF);
    REQUIRE(cpu.GetRegister(2) == 0xDEADBEEF);
    REQUIRE(cpu.GetRegister(1) == 0x02000000);
}

TEST_CASE("ARM SWPB swaps a byte atomically") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataSwap(0xE, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    cpu.SetRegister(2, 0xDEADBEAB);
    bus.Write8(0x02000000, 0x12);
    bus.Write8(0x02000001, 0x34);
    bus.Write8(0x02000002, 0x56);
    bus.Write8(0x02000003, 0x78);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x00000012);
    REQUIRE(bus.Read8(0x02000000) == 0xAB);
    REQUIRE(cpu.GetRegister(2) == 0xDEADBEAB);
    REQUIRE(cpu.GetRegister(1) == 0x02000000);
}

TEST_CASE("ARM SWPB zero-extends the loaded byte") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataSwap(0xE, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    cpu.SetRegister(0, 0xFFFFFFFF);
    bus.Write8(0x02000000, 0xF2);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x000000F2);
    REQUIRE((cpu.GetRegister(0) & 0xFFFFFF00) == 0);
}

TEST_CASE("ARM SWP handles Rm == Rn overlap correctly") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataSwap(0xE, false, 1, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write32(0x02000000, 0x11223344);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x11223344);
    REQUIRE(bus.Read32(0x02000000) == 0x02000000);
}

TEST_CASE("ARM SWP conditional execution fails when condition not met") {
    const u32 instruction = cpu_test::EncodeSingleDataSwap(0x0, false, 1, 0, 2);
    Bus bus(cpu_test::AsRom(instruction));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    cpu.SetRegister(2, 0xDEADBEEF);
    bus.Write32(0x02000000, 0x11223344);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0);
    REQUIRE(bus.Read32(0x02000000) == 0x11223344);
    REQUIRE(cpu.GetRegister(1) == 0x02000000);
    REQUIRE(cpu.GetRegister(2) == 0xDEADBEEF);
    REQUIRE(cpu.GetRegister(15) == 0x08000004);
}