#include <catch2/catch_test_macros.hpp>

#include "cpu_test_helpers.hpp"

TEST_CASE("ARM LDRT loads a word with post-indexed immediate offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeUserModeTransfer(0xE, true, false, 1, 2, 4)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write32(0x02000000, 0xA1B2C3D4);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0xA1B2C3D4);
    REQUIRE(cpu.GetRegister(1) == 0x02000004);
}

TEST_CASE("ARM STRT stores a word with post-indexed immediate offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeUserModeTransfer(0xE, false, false, 1, 0, 4)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x12345678);
    cpu.SetRegister(1, 0x02000000);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000000) == 0x12345678);
    REQUIRE(cpu.GetRegister(0) == 0x12345678);
    REQUIRE(cpu.GetRegister(1) == 0x02000004);
}

TEST_CASE("ARM LDRBT loads a byte zero-extended with post-indexed immediate offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeUserModeTransfer(0xE, true, true, 1, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    cpu.SetRegister(2, 0xFFFFFFFF);
    bus.Write8(0x02000000, 0xF2);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x000000F2);
    REQUIRE((cpu.GetRegister(2) & 0xFFFFFF00) == 0);
    REQUIRE(cpu.GetRegister(1) == 0x02000001);
}

TEST_CASE("ARM STRBT stores only the lowest byte with post-indexed immediate offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeUserModeTransfer(0xE, false, true, 1, 0, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x123456AB);
    cpu.SetRegister(1, 0x02000000);
    bus.Write8(0x02000000, 0x11);
    bus.Write8(0x02000001, 0x22);
    bus.Write8(0x02000002, 0x33);
    bus.Write8(0x02000003, 0x44);

    cpu.Step();

    REQUIRE(bus.Read8(0x02000000) == 0xAB);
    REQUIRE(bus.Read8(0x02000001) == 0x22);
    REQUIRE(bus.Read8(0x02000002) == 0x33);
    REQUIRE(bus.Read8(0x02000003) == 0x44);
    REQUIRE(cpu.GetRegister(0) == 0x123456AB);
    REQUIRE(cpu.GetRegister(1) == 0x02000001);
}

TEST_CASE("ARM LDRT uses negative post-indexed immediate offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeUserModeTransfer(0xE, true, false, 1, 2, -4)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000010);
    bus.Write32(0x02000010, 0xDEADBEEF);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0xDEADBEEF);
    REQUIRE(cpu.GetRegister(1) == 0x0200000C);
}

TEST_CASE("ARM STRT uses negative post-indexed immediate offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeUserModeTransfer(0xE, false, false, 1, 0, -4)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xCAFEBABE);
    cpu.SetRegister(1, 0x02000010);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000010) == 0xCAFEBABE);
    REQUIRE(cpu.GetRegister(0) == 0xCAFEBABE);
    REQUIRE(cpu.GetRegister(1) == 0x0200000C);
}

TEST_CASE("ARM LDRT with register offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeUserModeRegisterTransfer(0xE, true, false, 1, 2, 3)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    cpu.SetRegister(3, 0x10);
    bus.Write32(0x02000000, 0x99887766);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x99887766);
    REQUIRE(cpu.GetRegister(1) == 0x02000010);
}

TEST_CASE("ARM STRT with register offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeUserModeRegisterTransfer(0xE, false, false, 1, 0, 3)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x55667788);
    cpu.SetRegister(1, 0x02000000);
    cpu.SetRegister(3, 0x10);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000000) == 0x55667788);
    REQUIRE(cpu.GetRegister(1) == 0x02000010);
}

TEST_CASE("ARM LDRBT with register offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeUserModeRegisterTransfer(0xE, true, true, 1, 2, 3)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    cpu.SetRegister(3, 0x02);
    bus.Write8(0x02000000, 0x77);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x00000077);
    REQUIRE(cpu.GetRegister(1) == 0x02000002);
}

TEST_CASE("ARM STRBT with register offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeUserModeRegisterTransfer(0xE, false, true, 1, 0, 3)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x11223399);
    cpu.SetRegister(1, 0x02000000);
    cpu.SetRegister(3, 0x02);

    cpu.Step();

    REQUIRE(bus.Read8(0x02000000) == 0x99);
    REQUIRE(cpu.GetRegister(1) == 0x02000002);
}

TEST_CASE("ARM LDRT conditional execution fails when condition not met") {
    const u32 instruction = cpu_test::EncodeUserModeTransfer(0x0, true, false, 1, 2, 4);
    Bus bus(cpu_test::AsRom(instruction));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write32(0x02000000, 0x11223344);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0);
    REQUIRE(bus.Read32(0x02000000) == 0x11223344);
    REQUIRE(cpu.GetRegister(1) == 0x02000000);
    REQUIRE(cpu.GetRegister(15) == 0x08000004);
}