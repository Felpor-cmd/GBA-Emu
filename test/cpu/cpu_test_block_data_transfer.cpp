#include <catch2/catch_test_macros.hpp>

#include "cpu_test_helpers.hpp"

TEST_CASE("ARM STMIB stores registers in ascending address order without writeback") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x000E, true, true, false)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    cpu.SetRegister(2, 0x22222222);
    cpu.SetRegister(3, 0x33333333);

    cpu.Step();

    // IB mode: first store at base+4
    REQUIRE(bus.Read32(0x02000104) == 0x11111111);
    REQUIRE(bus.Read32(0x02000108) == 0x22222222);
    REQUIRE(bus.Read32(0x0200010C) == 0x33333333);
    REQUIRE(cpu.GetRegister(0) == 0x02000100);
}

TEST_CASE("ARM LDMIB loads registers in ascending address order without writeback") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, true, 0, 0x000E, true, true, false)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    bus.Write32(0x02000104, 0x11111111);
    bus.Write32(0x02000108, 0x22222222);
    bus.Write32(0x0200010C, 0x33333333);

    cpu.Step();

    // IB mode: first load from base+4
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

TEST_CASE("ARM STMDB stores registers in descending address order without writeback") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x000E, true, false, false)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    cpu.SetRegister(2, 0x22222222);
    cpu.SetRegister(3, 0x33333333);

    cpu.Step();

    // DB mode (P=1, U=0): first store at base-4, then base-8, base-12
    REQUIRE(bus.Read32(0x020000FC) == 0x11111111);
    REQUIRE(bus.Read32(0x020000F8) == 0x22222222);
    REQUIRE(bus.Read32(0x020000F4) == 0x33333333);
    REQUIRE(cpu.GetRegister(0) == 0x02000100);
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

    // DB mode (P=1, U=0): first store at base-4, then base-8, base-12
    REQUIRE(bus.Read32(0x020000FC) == 0x11111111);
    REQUIRE(bus.Read32(0x020000F8) == 0x22222222);
    REQUIRE(bus.Read32(0x020000F4) == 0x33333333);
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

    // DB mode (P=1, U=0): first store at base-4, then base-8, base-12
    // Writeback: base = base - register_count * 4 = 0x02000100 - 12 = 0x020000F4
    REQUIRE(bus.Read32(0x020000FC) == 0x11111111);
    REQUIRE(bus.Read32(0x020000F8) == 0x22222222);
    REQUIRE(bus.Read32(0x020000F4) == 0x33333333);
    REQUIRE(cpu.GetRegister(0) == 0x020000F4);
}

TEST_CASE("ARM STMIB packs a non-contiguous register list without writeback") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x008A, true, true, false)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    cpu.SetRegister(3, 0x33333333);
    cpu.SetRegister(7, 0x77777777);

    cpu.Step();

    // IB mode: first store at base+4 (0x02000104) for R1
    // then base+8 (0x02000108) for R3
    // then base+12 (0x0200010C) for R7
    REQUIRE(bus.Read32(0x02000104) == 0x11111111);
    REQUIRE(bus.Read32(0x02000108) == 0x33333333);
    REQUIRE(bus.Read32(0x0200010C) == 0x77777777);
    REQUIRE(cpu.GetRegister(0) == 0x02000100);
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

    // DB mode (P=1, U=0): first store at base-4, then base-8, base-12, base-16, base-20
    // Registers stored in order R4, R5, R6, R7, R14 (increasing register number)
    // SP becomes base - 20 = 0x03007EEC (points to R14, the last stored)
    REQUIRE(cpu.GetRegister(13) == 0x03007EEC);
    REQUIRE(bus.Read32(0x03007EFC) == 0x44444444);  // R4 at SP-4
    REQUIRE(bus.Read32(0x03007EF8) == 0x55555555);  // R5 at SP-8
    REQUIRE(bus.Read32(0x03007EF4) == 0x66666666);  // R6 at SP-12
    REQUIRE(bus.Read32(0x03007EF0) == 0x77777777);  // R7 at SP-16
    REQUIRE(bus.Read32(0x03007EEC) == 0xEEEEEEEE);  // R14 at SP-20
}

TEST_CASE("ARM LDMIA implements pop behavior with writeback") {
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

TEST_CASE("ARM LDMIA can load the program counter without writeback") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, true, 0, 0x8002, true, true, false)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    bus.Write32(0x02000104, 0x11111111);
    bus.Write32(0x02000108, 0x08000200);
    const u32 cpsr_before = cpu.GetCpsr();

    cpu.Step();

    REQUIRE(cpu.GetRegister(1) == 0x11111111);
    REQUIRE(cpu.GetRegister(15) == 0x08000200);
    REQUIRE(cpu.GetCpsr() == cpsr_before);
    REQUIRE(cpu.GetRegister(0) == 0x02000100);
}

TEST_CASE("ARM conditional block transfer does nothing when the condition fails") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0x0, false, 0, 0x0002, true, true, false)));
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

TEST_CASE("ARM block transfer with an empty register list loads/stores PC") {
    // Empty register list: stores/loads R15 (PC), adjusts base by +/- 0x40
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0, true, true, false)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    bus.Write32(0x02000100, 0xAAAAAAAA);

    cpu.Step();

    // Empty list (P=1, U=1 = IB mode): STM stores R15 (PC) at base+4, then base += 0x40
    // PC at execution = 0x08000004, stored at base+4 = 0x02000104
    REQUIRE(bus.Read32(0x02000104) == 0x08000004);
    REQUIRE(cpu.GetRegister(0) == 0x02000140);  // base + 0x40 (U=1)
    REQUIRE(cpu.GetRegister(1) == 0x11111111);
    REQUIRE(cpu.GetRegister(15) == 0x08000004);
}

TEST_CASE("ARM STMDA stores registers in descending address order with writeback") {
    // DA mode: P=0, U=0 (post-indexed, decrement after)
    // Writeback always happens with P=0
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x000E, false, false, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    cpu.SetRegister(2, 0x22222222);
    cpu.SetRegister(3, 0x33333333);

    cpu.Step();

    // DA mode (P=0, U=0): post-indexed decrement
    // First store at base (0x02000100), then base-4, base-8
    // Writeback: base = base - register_count*4 = 0x02000100 - 12 = 0x020000F4
    REQUIRE(bus.Read32(0x02000100) == 0x11111111);
    REQUIRE(bus.Read32(0x020000FC) == 0x22222222);
    REQUIRE(bus.Read32(0x020000F8) == 0x33333333);
    REQUIRE(cpu.GetRegister(0) == 0x020000F4);
}

TEST_CASE("ARM LDMDA loads registers in descending address order with writeback") {
    // DA mode: P=0, U=0 (post-indexed, decrement after)
    // Writeback always happens with P=0
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, true, 0, 0x000E, false, false, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    // DA mode: first load from base, then base-4, base-8
    bus.Write32(0x02000100, 0x11111111);
    bus.Write32(0x020000FC, 0x22222222);
    bus.Write32(0x020000F8, 0x33333333);

    cpu.Step();

    // DA mode: first load from base, then base-4, base-8
    REQUIRE(cpu.GetRegister(1) == 0x11111111);
    REQUIRE(cpu.GetRegister(2) == 0x22222222);
    REQUIRE(cpu.GetRegister(3) == 0x33333333);
    REQUIRE(cpu.GetRegister(0) == 0x020000F4);
}
