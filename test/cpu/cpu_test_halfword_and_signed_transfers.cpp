#include <catch2/catch_test_macros.hpp>

#include "cpu_test_helpers.hpp"

TEST_CASE("ARM LDRH loads a zero-extended halfword") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeHalfwordDataTransfer(
        0xE, true, false, true, 1, 2, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write16(0x02000002, 0xABCD);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x0000ABCD);
    REQUIRE(cpu.GetRegister(1) == 0x02000000);
}

TEST_CASE("ARM STRH stores only the low halfword") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeHalfwordDataTransfer(
        0xE, false, false, true, 1, 0, 4)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x1234ABCD);
    cpu.SetRegister(1, 0x02000000);
    bus.Write8(0x02000003, 0x11);
    bus.Write8(0x02000006, 0x22);

    cpu.Step();

    REQUIRE(bus.Read16(0x02000004) == 0xABCD);
    REQUIRE(bus.Read8(0x02000003) == 0x11);
    REQUIRE(bus.Read8(0x02000006) == 0x22);
}

TEST_CASE("ARM LDRSB preserves a positive signed byte") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeHalfwordDataTransfer(
        0xE, true, true, false, 1, 2, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write8(0x02000000, 0x7F);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x0000007F);
}

TEST_CASE("ARM LDRSB sign-extends a negative signed byte") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeHalfwordDataTransfer(
        0xE, true, true, false, 1, 2, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write8(0x02000000, 0x80);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0xFFFFFF80);
}

TEST_CASE("ARM LDRSH preserves a positive signed halfword") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeHalfwordDataTransfer(
        0xE, true, true, true, 1, 2, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write16(0x02000000, 0x1234);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x00001234);
}

TEST_CASE("ARM LDRSH sign-extends a negative signed halfword") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeHalfwordDataTransfer(
        0xE, true, true, true, 1, 2, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write16(0x02000000, 0x8000);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0xFFFF8000);
}

TEST_CASE("ARM LDRH applies a negative immediate offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeHalfwordDataTransfer(
        0xE, true, false, true, 1, 2, -4)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000010);
    bus.Write16(0x0200000C, 0xABCD);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x0000ABCD);
}

TEST_CASE("ARM LDRH pre-indexes with writeback") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeHalfwordDataTransfer(
        0xE, true, false, true, 1, 2, 4, true, true)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write16(0x02000004, 0xABCD);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x0000ABCD);
    REQUIRE(cpu.GetRegister(1) == 0x02000004);
}

TEST_CASE("ARM LDRSH post-indexes after loading from the old address") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeHalfwordDataTransfer(
        0xE, true, true, true, 1, 2, 2, false)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write16(0x02000000, 0x8000);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0xFFFF8000);
    REQUIRE(cpu.GetRegister(1) == 0x02000002);
}

TEST_CASE("ARM conditional halfword transfer does nothing when the condition fails") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeHalfwordDataTransfer(
        0x0, true, true, true, 1, 2, 4, true, true)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    cpu.SetRegister(2, 0xDEADBEEF);
    bus.Write16(0x02000004, 0x1234);

    cpu.Step();

    REQUIRE(bus.Read16(0x02000004) == 0x1234);
    REQUIRE(cpu.GetRegister(2) == 0xDEADBEEF);
    REQUIRE(cpu.GetRegister(1) == 0x02000000);
    REQUIRE(cpu.GetRegister(15) == 0x08000004);
}
