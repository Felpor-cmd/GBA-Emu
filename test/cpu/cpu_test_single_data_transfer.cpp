#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

#include "cpu_test_helpers.hpp"

TEST_CASE("ARM STR stores a word with a positive immediate offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, false, false,
                                                               1, 0, 4)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x12345678);
    cpu.SetRegister(1, 0x02000000);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000004) == 0x12345678);
    REQUIRE(cpu.GetRegister(0) == 0x12345678);
    REQUIRE(cpu.GetRegister(1) == 0x02000000);
}

TEST_CASE("ARM LDR loads a word with a positive immediate offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, true, false,
                                                               1, 2, 8)));
    Cpu cpu(bus);
    bus.Write32(0x02000008, 0xA1B2C3D4);
    cpu.SetRegister(1, 0x02000000);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0xA1B2C3D4);
    REQUIRE(cpu.GetRegister(1) == 0x02000000);
}

TEST_CASE("ARM STR stores a word with a negative immediate offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, false, false,
                                                               1, 0, -4)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xDEADBEEF);
    cpu.SetRegister(1, 0x02000010);

    cpu.Step();

    REQUIRE(bus.Read32(0x0200000C) == 0xDEADBEEF);
    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEF);
    REQUIRE(cpu.GetRegister(1) == 0x02000010);
}

TEST_CASE("ARM STRB stores only the lowest byte") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, false, true,
                                                               1, 0, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x123456AB);
    cpu.SetRegister(1, 0x02000000);
    bus.Write8(0x02000000, 0x11);
    bus.Write8(0x02000002, 0x22);
    bus.Write8(0x02000003, 0x33);

    cpu.Step();

    REQUIRE(bus.Read8(0x02000001) == 0xAB);
    REQUIRE(bus.Read8(0x02000000) == 0x11);
    REQUIRE(bus.Read8(0x02000002) == 0x22);
    REQUIRE(bus.Read8(0x02000003) == 0x33);
    REQUIRE(cpu.GetRegister(0) == 0x123456AB);
}

TEST_CASE("ARM LDRB zero-extends the loaded byte") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, true, true,
                                                               1, 2, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    cpu.SetRegister(2, 0xFFFFFFFF);
    bus.Write8(0x02000002, 0xF2);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x000000F2);
    REQUIRE((cpu.GetRegister(2) & 0xFFFFFF00) == 0);
}

TEST_CASE("ARM STR pre-indexes without writeback") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, false, false,
                                                               1, 0, 4)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xCAFEBABE);
    cpu.SetRegister(1, 0x02000000);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000004) == 0xCAFEBABE);
    REQUIRE(cpu.GetRegister(1) == 0x02000000);
}

TEST_CASE("ARM STR pre-indexes with writeback") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, false, false,
                                                               1, 0, 4, true, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xCAFEBABE);
    cpu.SetRegister(1, 0x02000000);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000004) == 0xCAFEBABE);
    REQUIRE(cpu.GetRegister(1) == 0x02000004);
}

TEST_CASE("ARM STR post-indexes using the old address") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, false, false,
                                                               1, 0, 4, false)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xCAFEBABE);
    cpu.SetRegister(1, 0x02000000);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000000) == 0xCAFEBABE);
    REQUIRE(cpu.GetRegister(1) == 0x02000004);
}

TEST_CASE("ARM LDR post-indexes after loading from the old address") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, true, false,
                                                               1, 2, 4, false)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write32(0x02000000, 0x11223344);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x11223344);
    REQUIRE(cpu.GetRegister(1) == 0x02000004);
}

TEST_CASE("ARM STR uses an unshifted register offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeRegisterDataTransfer(0xE, false, false,
                                                                 1, 0, 3)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x55667788);
    cpu.SetRegister(1, 0x02000000);
    cpu.SetRegister(3, 0x10);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000010) == 0x55667788);
}

TEST_CASE("ARM LDR uses an unshifted register offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeRegisterDataTransfer(0xE, true, false,
                                                                 1, 2, 3)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    cpu.SetRegister(3, 0x10);
    bus.Write32(0x02000010, 0x99887766);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x99887766);
}

TEST_CASE("ARM LDR uses a shifted register offset") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeRegisterDataTransfer(0xE, true, false,
                                                                 1, 2, 3, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    cpu.SetRegister(3, 4);
    bus.Write32(0x02000010, 0xABCDEF01);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0xABCDEF01);
}

TEST_CASE("ARM conditional transfer does nothing when the condition fails") {
    const u32 instruction = cpu_test::EncodeSingleDataTransfer(0x0, false, false,
                                                               1, 0, 4, true, true);
    Bus bus(cpu_test::AsRom(instruction));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xDEADBEEF);
    cpu.SetRegister(1, 0x02000000);
    bus.Write32(0x02000004, 0x11223344);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000004) == 0x11223344);
    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEF);
    REQUIRE(cpu.GetRegister(1) == 0x02000000);
    REQUIRE(cpu.GetRegister(15) == 0x08000004);
}

TEST_CASE("ARM LDR uses the architectural PC value for PC-relative addressing") {
    const u32 instruction = cpu_test::EncodeSingleDataTransfer(0xE, true, false,
                                                               15, 0, 0);
    std::vector<u8> rom(0x0C, 0);
    cpu_test::Put32(rom, 0, instruction);
    cpu_test::Put32(rom, 8, 0xCAFED00D);
    Bus bus(std::move(rom));
    Cpu cpu(bus);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xCAFED00D);
}
