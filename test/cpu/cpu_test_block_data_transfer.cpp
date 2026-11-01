#include <catch2/catch_test_macros.hpp>

#include "cpu_test_helpers.hpp"

TEST_CASE("ARM STM stores PC as instruction address plus twelve") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x8002, false, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x12345678);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000100) == 0x12345678);
    REQUIRE(bus.Read32(0x02000104) == 0x0800000C);
    REQUIRE(cpu.GetRegister(0) == 0x02000100);
}

TEST_CASE("ARM STM with S bit stores User registers from FIQ mode") {
    std::vector<u8> rom(8, 0);
    cpu_test::Put32(rom, 0, 0xE121F003u);  // MSR CPSR_c, r3
    cpu_test::Put32(rom, 4, cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x6300, false, true, false, true));  // r8, r9, r13, r14
    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(3, 0x11);  // FIQ
    cpu.SetRegister(8, 0x11111111);
    cpu.SetRegister(9, 0x22222222);
    cpu.SetRegister(13, 0x33333333);
    cpu.SetRegister(14, 0x44444444);
    cpu.Step();
    cpu.SetRegister(8, 0xAAAAAAAA);
    cpu.SetRegister(9, 0xBBBBBBBB);
    cpu.SetRegister(13, 0xCCCCCCCC);
    cpu.SetRegister(14, 0xDDDDDDDD);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000100) == 0x11111111);
    REQUIRE(bus.Read32(0x02000104) == 0x22222222);
    REQUIRE(bus.Read32(0x02000108) == 0x33333333);
    REQUIRE(bus.Read32(0x0200010C) == 0x44444444);
    REQUIRE(cpu.GetRegister(8) == 0xAAAAAAAA);
    REQUIRE(cpu.GetRegister(13) == 0xCCCCCCCC);
    REQUIRE(cpu.GetCpsr() == 0x11);
}

TEST_CASE("ARM STM with S bit selects User SP and LR from IRQ mode") {
    std::vector<u8> rom(8, 0);
    cpu_test::Put32(rom, 0, 0xE121F003u);  // MSR CPSR_c, r3
    cpu_test::Put32(rom, 4, cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x6000, false, true, false, true));
    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(3, 0x12);
    cpu.SetRegister(13, 0x11111111);
    cpu.SetRegister(14, 0x22222222);
    cpu.Step();
    cpu.SetRegister(13, 0xAAAAAAAA);
    cpu.SetRegister(14, 0xBBBBBBBB);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000100) == 0x11111111);
    REQUIRE(bus.Read32(0x02000104) == 0x22222222);
    REQUIRE(cpu.GetRegister(13) == 0xAAAAAAAA);
    REQUIRE(cpu.GetRegister(14) == 0xBBBBBBBB);
}

TEST_CASE("ARM LDM with S bit loads User registers without changing FIQ bank") {
    std::vector<u8> rom(12, 0);
    cpu_test::Put32(rom, 0, 0xE121F003u);  // MSR CPSR_c, r3
    cpu_test::Put32(rom, 4, cpu_test::EncodeBlockDataTransfer(
        0xE, true, 0, 0x2100, false, true, false, true));  // r8, r13
    cpu_test::Put32(rom, 8, 0xE121F003u);  // return to System mode
    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(3, 0x11);
    bus.Write32(0x02000100, 0x12345678);
    bus.Write32(0x02000104, 0x87654321);
    cpu.Step();
    cpu.SetRegister(8, 0xAAAAAAAA);
    cpu.SetRegister(13, 0xBBBBBBBB);

    cpu.Step();

    REQUIRE(cpu.GetRegister(8) == 0xAAAAAAAA);
    REQUIRE(cpu.GetRegister(13) == 0xBBBBBBBB);
    REQUIRE(cpu.GetCpsr() == 0x11);
    cpu.SetRegister(3, 0x1F);
    cpu.Step();
    REQUIRE(cpu.GetRegister(8) == 0x12345678);
    REQUIRE(cpu.GetRegister(13) == 0x87654321);
}

TEST_CASE("ARM LDM with PC and S bit returns to Thumb at a halfword address") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, true, 0, 0x8000, false, true, false, true)));
    Cpu cpu(bus);
    cpu.SetCpsr(0x12);  // IRQ mode
    cpu.SetSpsr(0x6000003Fu);  // System mode, Thumb state, flags
    cpu.SetRegister(0, 0x02000100);
    bus.Write32(0x02000100, 0x02000002);
    bus.Write16(0x02000002, 0x0001);  // LSL r1, r0, #0

    cpu.Step();

    REQUIRE(cpu.GetCpsr() == 0x6000003Fu);
    REQUIRE(cpu.GetRegister(15) == 0x02000002);
    cpu.Step();
    REQUIRE(cpu.GetRegister(15) == 0x02000004);
}

TEST_CASE("ARM LDM exception return writes back to the old mode bank") {
    std::vector<u8> rom(20, 0);
    cpu_test::Put32(rom, 0, 0xE121F003u);  // MSR CPSR_c, r3
    cpu_test::Put32(rom, 4, cpu_test::EncodeBlockDataTransfer(
        0xE, true, 13, 0xC000, false, true, true, true));  // LDMIA r13!, {r14,pc}^
    cpu_test::Put32(rom, 16, 0xE121F003u);  // switch back to IRQ
    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(3, 0x12);
    cpu.SetRegister(14, 0xAAAAAAAA);
    cpu.Step();
    cpu.SetRegister(13, 0x02000100);
    cpu.SetRegister(14, 0xBBBBBBBB);
    cpu.SetSpsr(0xA000001Fu);
    bus.Write32(0x02000100, 0x12345678);
    bus.Write32(0x02000104, 0x08000010);

    cpu.Step();

    REQUIRE(cpu.GetCpsr() == 0xA000001F);
    REQUIRE(cpu.GetRegister(13) == 0x03007F00);
    REQUIRE(cpu.GetRegister(14) == 0xAAAAAAAA);
    REQUIRE(cpu.GetRegister(15) == 0x08000010);
    cpu.Step();
    REQUIRE(cpu.GetRegister(13) == 0x02000108);
    REQUIRE(cpu.GetRegister(14) == 0x12345678);
}

TEST_CASE("ARM LDM with PC and S bit loads the exception bank before returning") {
    std::vector<u8> rom(20, 0);
    cpu_test::Put32(rom, 0, 0xE121F003u);  // MSR CPSR_c, r3
    cpu_test::Put32(rom, 4, cpu_test::EncodeBlockDataTransfer(
        0xE, true, 0, 0x8100, false, true, false, true));  // r8, pc
    cpu_test::Put32(rom, 16, 0xE121F003u);
    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(3, 0x11);
    cpu.SetRegister(8, 0x11111111);  // User r8
    cpu.Step();
    cpu.SetRegister(8, 0xAAAAAAAA);  // FIQ r8
    cpu.SetSpsr(0x1F);
    bus.Write32(0x02000100, 0xBBBBBBBB);
    bus.Write32(0x02000104, 0x08000010);

    cpu.Step();

    REQUIRE(cpu.GetCpsr() == 0x1F);
    REQUIRE(cpu.GetRegister(8) == 0x11111111);
    REQUIRE(cpu.GetRegister(15) == 0x08000010);
    cpu.Step();
    REQUIRE(cpu.GetRegister(8) == 0xBBBBBBBB);
}

TEST_CASE("ARM STMDB maps the lowest register to the lowest address") {
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

TEST_CASE("ARM STM with base in list stores the ARMv4 base value and writes back") {
    SECTION("base is first register") {
        Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
            0xE, false, 0, 0x0003, false, true, true)));
        Cpu cpu(bus);
        cpu.SetRegister(0, 0x02000100);
        cpu.SetRegister(1, 0x11111111);
        cpu.Step();
        REQUIRE(bus.Read32(0x02000100) == 0x02000100);
        REQUIRE(cpu.GetRegister(0) == 0x02000108);
    }
    SECTION("base follows another register") {
        Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
            0xE, false, 1, 0x0003, false, true, true)));
        Cpu cpu(bus);
        cpu.SetRegister(0, 0x11111111);
        cpu.SetRegister(1, 0x02000100);
        cpu.Step();
        REQUIRE(bus.Read32(0x02000100) == 0x11111111);
        REQUIRE(bus.Read32(0x02000104) == 0x02000108);
        REQUIRE(cpu.GetRegister(1) == 0x02000108);
    }
}

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

TEST_CASE("ARM STMDB stores registers in ascending address order without writeback") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x000E, true, false, false)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    cpu.SetRegister(2, 0x22222222);
    cpu.SetRegister(3, 0x33333333);

    cpu.Step();

    // DB mode spans base-12 through base-4, with the lowest register first.
    REQUIRE(bus.Read32(0x020000F4) == 0x11111111);
    REQUIRE(bus.Read32(0x020000F8) == 0x22222222);
    REQUIRE(bus.Read32(0x020000FC) == 0x33333333);
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

    // Writeback: base = base - register_count * 4 = 0x02000100 - 12 = 0x020000F4
    REQUIRE(bus.Read32(0x020000F4) == 0x11111111);
    REQUIRE(bus.Read32(0x020000F8) == 0x22222222);
    REQUIRE(bus.Read32(0x020000FC) == 0x33333333);
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

    // R4 occupies the new SP; R14 is at the highest address.
    REQUIRE(cpu.GetRegister(13) == 0x03007EEC);
    REQUIRE(bus.Read32(0x03007EEC) == 0x44444444);
    REQUIRE(bus.Read32(0x03007EF0) == 0x55555555);
    REQUIRE(bus.Read32(0x03007EF4) == 0x66666666);
    REQUIRE(bus.Read32(0x03007EF8) == 0x77777777);
    REQUIRE(bus.Read32(0x03007EFC) == 0xEEEEEEEE);
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
        0xE, false, 0, 0, true, true, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    bus.Write32(0x02000100, 0xAAAAAAAA);

    cpu.Step();

    // Empty list (P=1, U=1 = IB mode): STM stores R15 (PC) at base+4, then base += 0x40
    REQUIRE(bus.Read32(0x02000104) == 0x0800000C);
    REQUIRE(cpu.GetRegister(0) == 0x02000140);  // base + 0x40 (U=1)
    REQUIRE(cpu.GetRegister(1) == 0x11111111);
    REQUIRE(cpu.GetRegister(15) == 0x08000004);
}

TEST_CASE("ARM empty register list leaves base unchanged without W bit") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0, false, true, false)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000100) == 0x0800000C);
    REQUIRE(cpu.GetRegister(0) == 0x02000100);
}

TEST_CASE("ARM STMDA stores registers in ascending address order with writeback") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, false, 0, 0x000E, false, false, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    cpu.SetRegister(1, 0x11111111);
    cpu.SetRegister(2, 0x22222222);
    cpu.SetRegister(3, 0x33333333);

    cpu.Step();

    // DA mode spans base-8 through base.
    REQUIRE(bus.Read32(0x020000F8) == 0x11111111);
    REQUIRE(bus.Read32(0x020000FC) == 0x22222222);
    REQUIRE(bus.Read32(0x02000100) == 0x33333333);
    REQUIRE(cpu.GetRegister(0) == 0x020000F4);
}

TEST_CASE("ARM LDMDA loads registers in ascending address order with writeback") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
        0xE, true, 0, 0x000E, false, false, true)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x02000100);
    bus.Write32(0x020000F8, 0x11111111);
    bus.Write32(0x020000FC, 0x22222222);
    bus.Write32(0x02000100, 0x33333333);

    cpu.Step();

    REQUIRE(cpu.GetRegister(1) == 0x11111111);
    REQUIRE(cpu.GetRegister(2) == 0x22222222);
    REQUIRE(cpu.GetRegister(3) == 0x33333333);
    REQUIRE(cpu.GetRegister(0) == 0x020000F4);
}
