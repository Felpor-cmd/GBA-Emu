#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <utility>

#include "cpu_test_helpers.hpp"

TEST_CASE("Reserved ARMv5 and unallocated words enter Undefined only when executed") {
    const u32 words[] = {0xE12FFF30u, 0xE16F0F10u, 0xE1200070u,
                         0xE1C000D0u, 0xE1C000F0u, 0xE1003281u,
                         0xE7900011u};
    for (u32 word : words) {
        Bus bus(cpu_test::AsRom(word));
        Cpu cpu(bus);
        cpu.Step();
        REQUIRE((cpu.GetCpsr() & 31u) == 0x1Bu);
        REQUIRE(cpu.GetRegister(15) == 4);
        REQUIRE(cpu.GetRegister(14) == 0x08000004u);
    }

    Bus bus(cpu_test::AsRom(0x012FFF30u)); // BLXEQ, with Z clear
    Cpu cpu(bus);
    REQUIRE(cpu.Step() == 8);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
    REQUIRE((cpu.GetCpsr() & 31u) == 0x1Fu);
}

TEST_CASE("Unpredictable multiply operand combination remains a no-op") {
    // MUL r0,r0,r1 has Rd == Rm, which this emulator deliberately ignores.
    Bus bus(cpu_test::AsRom(0xE0000190u));
    Cpu cpu(bus);
    cpu.SetRegister(0, 7);
    cpu.SetRegister(1, 3);
    cpu.Step();
    REQUIRE(cpu.GetRegister(0) == 7);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
    REQUIRE((cpu.GetCpsr() & 31u) == 0x1Fu);
}

TEST_CASE("SWP read and write each charge nonsequential data clocks") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataSwap(0xE, false, 1, 2, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xAABBCCDDu);
    cpu.SetRegister(1, 0x02000000u);
    bus.Write32(0x02000000u, 0x12345678u);
    REQUIRE(cpu.Step() == 21); // ROM fetch 8 + two EWRAM words 6 each + I cycle
    REQUIRE(cpu.GetRegister(2) == 0x12345678u);
    REQUIRE(bus.Read32(0x02000000u) == 0xAABBCCDDu);
}

TEST_CASE("Instruction fetch, load, store and cumulative clocks use bus widths") {
    std::vector<u8> rom(12);
    cpu_test::Put32(rom, 0, 0xE1A00000u); // MOV r0,r0
    cpu_test::Put32(rom, 4, cpu_test::EncodeSingleDataTransfer(0xE, true, false, 1, 2, 0));
    cpu_test::Put32(rom, 8, cpu_test::EncodeSingleDataTransfer(0xE, false, false, 1, 2, 4));
    Bus bus(rom);
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x03000000u);
    bus.Write32(0x03000000u, 0x12345678u);
    REQUIRE(cpu.Step() == 8); // first ROM word: N + S halfwords
    REQUIRE(cpu.Step() == 8); // sequential ROM word + data + load internal
    REQUIRE(cpu.GetRegister(2) == 0x12345678u);
    REQUIRE(cpu.Step() == 9); // data access breaks ROM sequential fetch
    REQUIRE(bus.Read32(0x03000004u) == 0x12345678u);
    REQUIRE(cpu.GetCycles() == 25);
}

TEST_CASE("Multiply clocks depend on Rs and accumulation") {
    for (auto [rs, expected] : {std::pair{1u, 9u}, std::pair{0x100u, 10u},
                                std::pair{0x10000u, 11u}, std::pair{0x1000000u, 12u}}) {
        // MUL r2,r0,r1
        Bus bus(cpu_test::AsRom(0xE0020190u));
        Cpu cpu(bus);
        cpu.SetRegister(0, 3);
        cpu.SetRegister(1, rs);
        REQUIRE(cpu.Step() == expected);
        REQUIRE(cpu.GetRegister(2) == 3u * rs);
    }
}

TEST_CASE("Multiply timing uses Rs before a destination overwrites it") {
    // MUL r1,r0,r1 is architecturally valid; Rs is also Rd.
    Bus bus(cpu_test::AsRom(0xE0010190u));
    Cpu cpu(bus);
    cpu.SetRegister(0, 2);
    cpu.SetRegister(1, 0x01000000u);
    REQUIRE(cpu.Step() == 12);
    REQUIRE(cpu.GetRegister(1) == 0x02000000u);
}

TEST_CASE("Block transfer clocks scale with register count") {
    for (u16 mask : {static_cast<u16>(1), static_cast<u16>(3), static_cast<u16>(15)}) {
        const u32 count = static_cast<u32>(std::popcount(static_cast<unsigned>(mask)));
        for (bool load : {false, true}) {
            Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(
                0xE, load, 1, mask, false, true)));
            Cpu cpu(bus);
            cpu.SetRegister(1, 0x03000000u);
            REQUIRE(cpu.Step() == 8 + count + (load ? 1u : 0u));
        }
    }
}

TEST_CASE("Branches, PC writes and exceptions charge two refill accesses") {
    for (u32 instruction : {0xEA000000u, 0xE1A0F000u, 0xEF000000u, 0xE12FFF30u}) {
        Bus bus(cpu_test::AsRom(instruction));
        Cpu cpu(bus);
        cpu.SetRegister(0, 0x03000000u);
        const u32 expected = instruction == 0xE1A0F000u ? 10u :
                             (instruction == 0xEF000000u || instruction == 0xE12FFF30u) ? 10u : 22u;
        REQUIRE(cpu.Step() == expected);
    }
}

TEST_CASE("ARM instruction families have their core internal cycle schedules") {
    const u32 ldrh = cpu_test::EncodeHalfwordDataTransfer(0xE, true, false, true, 1, 2, 0);
    const std::pair<u32, u32> cases[] = {
        {0xE1A00000u, 8},  // data processing
        {0xE1A00110u, 9},  // register-specified shift
        {0xE10F0000u, 8},  // MRS
        {0xE128F000u, 8},  // MSR
        {0xE0423190u, 10}, // UMULL with one multiply iteration
        {ldrh, 10},        // halfword load and result cycle
        {0xE12FFF10u, 10} // BX to IWRAM
    };
    for (auto [word, clocks] : cases) {
        Bus bus(cpu_test::AsRom(word));
        Cpu cpu(bus);
        cpu.SetRegister(0, 0x03000000u);
        cpu.SetRegister(1, 0x03000000u);
        // UMULL needs a small Rs value to terminate after one cycle.
        if (word == 0xE0423190u) cpu.SetRegister(1, 1);
        REQUIRE(cpu.Step() == clocks);
    }
}

TEST_CASE("WAITCNT and 128 KiB boundaries affect Game Pak access clocks") {
    Bus bus({});
    REQUIRE(bus.AccessCycles(0x08000002u, 2, false) == 5);
    REQUIRE(bus.AccessCycles(0x08000002u, 2, true) == 3);
    REQUIRE(bus.AccessCycles(0x08000000u, 4, false) == 8);
    REQUIRE(bus.AccessCycles(0x08020000u, 2, true) == 5);
    bus.Write16(0x04000204u, 0x4317u);
    REQUIRE(bus.GetWaitcnt() == 0x4317u);
    REQUIRE(bus.PrefetchEnabled());
    REQUIRE(bus.AccessCycles(0x08000002u, 2, false) == 4);
    REQUIRE(bus.AccessCycles(0x08000002u, 2, true) == 2);
    REQUIRE(bus.AccessCycles(0x0A000002u, 2, false) == 5);
}

TEST_CASE("ROM prefetch uses idle multiply cycles for the next opcode") {
    std::vector<u8> rom(8);
    cpu_test::Put32(rom, 0, 0xE0020190u); // MUL r2,r0,r1
    cpu_test::Put32(rom, 4, 0xE1A03003u); // MOV r3,r3
    Bus bus(rom);
    bus.Write16(0x04000204u, 0x4317u);
    Cpu cpu(bus);
    cpu.SetRegister(0, 2);
    cpu.SetRegister(1, 0x01000000u);
    REQUIRE(cpu.Step() == 10); // fetch 6, four internal multiply clocks
    REQUIRE(cpu.Step() == 1);  // two prefetched halfwords
}

TEST_CASE("ARM store to WAITCNT changes following opcode fetch timing") {
    std::vector<u8> rom(8);
    cpu_test::Put32(rom, 0, cpu_test::EncodeHalfwordDataTransfer(
        0xE, false, false, true, 1, 0, 0));
    cpu_test::Put32(rom, 4, 0xE1A02002u);
    Bus bus(rom);
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x4317u);
    cpu.SetRegister(1, 0x04000204u);
    REQUIRE(cpu.Step() == 9);
    REQUIRE(bus.GetWaitcnt() == 0x4317u);
    REQUIRE(cpu.Step() == 6);
    REQUIRE(cpu.GetCycles() == 15);
}

TEST_CASE("Sequential ROM fetch becomes nonsequential at 128 KiB boundary") {
    std::vector<u8> rom(0x20004u);
    cpu_test::Put32(rom, 0x1FFFCu, 0xE1A00000u);
    cpu_test::Put32(rom, 0x20000u, 0xE1A00000u);
    Bus bus(rom);
    Cpu cpu(bus);
    cpu.SetRegister(15, 0x0801FFFCu);
    REQUIRE(cpu.Step() == 8);
    REQUIRE(cpu.Step() == 8);
}
