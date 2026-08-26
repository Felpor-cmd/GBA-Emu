#include <catch2/catch_test_macros.hpp>

#include "cpu_test_helpers.hpp"

TEST_CASE("ARM STMIA stores registers in ascending address order") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x000E, false, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    cpu.SetRegister(2, 0x22222222);
    cpu.SetRegister(3, 0x33333333);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000100) == 0x11111111);
    REQUIRE(bus.Read32(0x02000104) == 0x22222222);
    REQUIRE(bus.Read32(0x02000108) == 0x33333333);
    REQUIRE(cpu.GetRegister(0) == 0x02000100);
}

TEST_CASE("ARM LDMIA loads registers in ascending address order") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, true, 0, 0x000E, false, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    bus.Write32(0x02000100, 0x11111111);
    bus.Write32(0x02000104, 0x22222222);
    bus.Write32(0x02000108, 0x33333333);

    cpu.Step();

    REQUIRE(cpu.GetRegister(1) == 0x11111111);
    REQUIRE(cpu.GetRegister(2) == 0x22222222);
    REQUIRE(cpu.GetRegister(3) == 0x33333333);
    REQUIRE(cpu.GetRegister(0) == 0x02000100);
}

TEST_CASE("ARM STMIB increments before storing") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x000E, true, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    cpu.SetRegister(2, 0x22222222);
    cpu.SetRegister(3, 0x33333333);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000104) == 0x11111111);
    REQUIRE(bus.Read32(0x02000108) == 0x22222222);
    REQUIRE(bus.Read32(0x0200010C) == 0x33333333);
    REQUIRE(cpu.GetRegister(0) == 0x02000100);
}

TEST_CASE("ARM STMDA decrements after storing") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x000E, false, false)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    cpu.SetRegister(2, 0x22222222);
    cpu.SetRegister(3, 0x33333333);

    cpu.Step();

    REQUIRE(bus.Read32(0x020000F8) == 0x11111111);
    REQUIRE(bus.Read32(0x020000FC) == 0x22222222);
    REQUIRE(bus.Read32(0x02000100) == 0x33333333);
}

TEST_CASE("ARM STMDB decrements before storing") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x000E, true, false)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    cpu.SetRegister(2, 0x22222222);
    cpu.SetRegister(3, 0x33333333);

    cpu.Step();

    REQUIRE(bus.Read32(0x020000F4) == 0x11111111);
    REQUIRE(bus.Read32(0x020000F8) == 0x22222222);
    REQUIRE(bus.Read32(0x020000FC) == 0x33333333);
}

TEST_CASE("ARM STMIA writes back the incremented base") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x000E, false, true, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    cpu.SetRegister(2, 0x22222222);
    cpu.SetRegister(3, 0x33333333);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000100) == 0x11111111);
    REQUIRE(bus.Read32(0x02000108) == 0x33333333);
    REQUIRE(cpu.GetRegister(0) == 0x0200010C);
}

TEST_CASE("ARM STMDB writes back the decremented base") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x000E, true, false, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    cpu.SetRegister(2, 0x22222222);
    cpu.SetRegister(3, 0x33333333);

    cpu.Step();

    REQUIRE(bus.Read32(0x020000F4) == 0x11111111);
    REQUIRE(bus.Read32(0x020000FC) == 0x33333333);
    REQUIRE(cpu.GetRegister(0) == 0x020000F4);
}

TEST_CASE("ARM STMIA packs a non-contiguous register list") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x008A, false, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    cpu.SetRegister(3, 0x33333333);
    cpu.SetRegister(7, 0x77777777);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000100) == 0x11111111);
    REQUIRE(bus.Read32(0x02000104) == 0x33333333);
    REQUIRE(bus.Read32(0x02000108) == 0x77777777);
}

TEST_CASE("ARM STMDB implements push behavior") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 13, 0x40F0, true, false, true)));
    Cpu cpu(bus);
    cpu.SetRegister(13, 0x03007F00);
    cpu.SetRegister(4, 0x44444444);
    cpu.SetRegister(5, 0x55555555);
    cpu.SetRegister(6, 0x66666666);
    cpu.SetRegister(7, 0x77777777);
    cpu.SetRegister(14, 0xEEEEEEEE);

    cpu.Step();

    REQUIRE(cpu.GetRegister(13) == 0x03007EEC);
    REQUIRE(bus.Read32(0x03007EEC) == 0x44444444);
    REQUIRE(bus.Read32(0x03007EF0) == 0x55555555);
    REQUIRE(bus.Read32(0x03007EF4) == 0x66666666);
    REQUIRE(bus.Read32(0x03007EF8) == 0x77777777);
    REQUIRE(bus.Read32(0x03007EFC) == 0xEEEEEEEE);
}

TEST_CASE("ARM LDMIA implements pop behavior") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, true, 13, 0x40F0, false, true, true)));
    Cpu cpu(bus);
    cpu.SetRegister(13, 0x03007EEC);
    bus.Write32(0x03007EEC, 0x44444444);
    bus.Write32(0x03007EF0, 0x55555555);
    bus.Write32(0x03007EF4, 0x66666666);
    bus.Write32(0x03007EF8, 0x77777777);
    bus.Write32(0x03007EFC, 0xEEEEEEEE);

    cpu.Step();

    REQUIRE(cpu.GetRegister(4) == 0x44444444);
    REQUIRE(cpu.GetRegister(5) == 0x55555555);
    REQUIRE(cpu.GetRegister(6) == 0x66666666);
    REQUIRE(cpu.GetRegister(7) == 0x77777777);
    REQUIRE(cpu.GetRegister(14) == 0xEEEEEEEE);
    REQUIRE(cpu.GetRegister(13) == 0x03007F00);
}

TEST_CASE("ARM LDMIA can load the program counter") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, true, 0, 0x8002, false, true, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    bus.Write32(0x02000100, 0x11111111);
    bus.Write32(0x02000104, 0x08000200);
    const u32 cpsr_before = cpu.GetCpsr();

    cpu.Step();

    REQUIRE(cpu.GetRegister(1) == 0x11111111);
    REQUIRE(cpu.GetRegister(15) == 0x08000200);
    REQUIRE(cpu.GetCpsr() == cpsr_before);
}

TEST_CASE("ARM conditional block transfer does nothing when the condition fails") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0x0, false, 0, 0x0002, false, true, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    bus.Write32(0x02000100, 0xAAAAAAAA);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000100) == 0xAAAAAAAA);
    REQUIRE(cpu.GetRegister(0) == 0x02000100);
    REQUIRE(cpu.GetRegister(1) == 0x11111111);
    REQUIRE(cpu.GetRegister(15) == 0x08000004);
}

TEST_CASE("ARM block transfer with an empty register list is unsupported") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0, false, true, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    bus.Write32(0x02000100, 0xAAAAAAAA);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000100) == 0xAAAAAAAA);
    REQUIRE(cpu.GetRegister(0) == 0x02000100);
    REQUIRE(cpu.GetRegister(1) == 0x11111111);
    REQUIRE(cpu.GetRegister(15) == 0x08000004);
}
