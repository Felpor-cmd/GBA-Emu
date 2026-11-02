#include <catch2/catch_test_macros.hpp>
#include "cpu_test_helpers.hpp"
#include "emulator.hpp"
#include "ppu/ppu.hpp"
#include <stdexcept>

TEST_CASE("Supplied BIOS must have the exact GBA ROM size") {
    REQUIRE_THROWS_AS(Bus(cpu_test::AsRom(0), std::vector<u8>(kBiosSize - 1)), std::invalid_argument);
    REQUIRE_THROWS_AS(Bus(cpu_test::AsRom(0), std::vector<u8>(kBiosSize + 1)), std::invalid_argument);
}

TEST_CASE("Power-on enters the supplied BIOS before cartridge startup") {
    std::vector<u8> bios(kBiosSize);
    cpu_test::Put32(bios, 0, 0xEA000006u); // B 0x20.
    cpu_test::Put32(bios, 0x20, 0xE59FF000u); // LDR pc,[pc].
    cpu_test::Put32(bios, 0x28, 0x08000000u);
    Emulator emulator(cpu_test::AsRom(0xE3A0002Au), true, bios);
    REQUIRE(emulator.Pc() == 0);
    Bus bus(cpu_test::AsRom(0xE3A0002Au), std::move(bios));
    Cpu cpu(bus, true);
    cpu.PowerOn();
    REQUIRE(cpu.GetCpsr() == 0xD3u);
    REQUIRE(cpu.GetRegister(15) == 0);
    cpu.Step();
    REQUIRE(cpu.GetRegister(15) == 0x20);
    cpu.Step();
    REQUIRE(cpu.GetRegister(15) == 0x08000000u);
    cpu.Step();
    REQUIRE(cpu.GetRegister(0) == 42);
    REQUIRE(bus.Read8(0x04000300u) == 0);
}

TEST_CASE("BIOS reads outside BIOS execution expose the last prefetched opcode") {
    std::vector<u8> bios(kBiosSize);
    cpu_test::Put32(bios, 0, 0xE59F0000u); // LDR r0,[pc], prefetched word at 8.
    cpu_test::Put32(bios, 8, 0x12345678u);
    cpu_test::Put32(bios, 12, 0xE59FF000u); // LDR pc,[pc].
    cpu_test::Put32(bios, 20, 0x08000000u);
    Bus bus(cpu_test::AsRom(0xE1A00000u), std::move(bios));
    Cpu cpu(bus);
    cpu.PowerOn();
    cpu.Step();
    REQUIRE(cpu.GetRegister(0) == 0x12345678u);
    cpu.SetRegister(15, 12);
    cpu.Step();
    REQUIRE(cpu.GetRegister(15) == 0x08000000u);
    REQUIRE(bus.Read32(0) == 0x08000000u); // Prefetch at 0x14, not BIOS word at zero.
    REQUIRE(bus.Read8(2) == 0x00u);
    REQUIRE(bus.Read32(0x4000) == 0x08000000u); // Unmapped memory exposes the CPU open bus.
}

TEST_CASE("POSTFLG latches and HALTCNT sleeps until an enabled interrupt") {
    Bus bus(cpu_test::AsRom(0xE1A00000u), std::vector<u8>(kBiosSize));
    Cpu cpu(bus);
    bus.Write8(0x04000300u, 1); // Cartridge writes cannot set POSTFLG.
    REQUIRE(bus.Read8(0x04000300u) == 0);
    bus.SetCpuExecutionAddress(0);
    bus.Write8(0x04000300u, 1);
    bus.Write8(0x04000300u, 0);
    REQUIRE(bus.Read8(0x04000300u) == 1);
    bus.Write8(0x04000301u, 0);
    bus.SetCpuExecutionAddress(0x08000000u);
    cpu.Step();
    REQUIRE(cpu.IsHalted());
    bus.Write16(0x04000200u, 1);
    bus.RequestInterrupt(1);
    cpu.Step();
    REQUIRE_FALSE(cpu.IsHalted());
}

TEST_CASE("VBlankIntrWait dispatches the game IRQ and returns to Thumb with context intact") {
    std::vector<u8> rom(0x140, 0);
    cpu_test::Put32(rom, 0, 0x222ADF05u); // SWI 5; MOV r2,42
    const u32 handler[] = {0xE59F0018u, 0xE3A01001u, 0xE1C010B0u,
        0xE59F0010u, 0xE1D010B0u, 0xE3811001u, 0xE1C010B0u, 0xE12FFF1Eu,
        0x04000202u, 0x03007FF8u};
    for (u32 i = 0; i < 10; ++i) cpu_test::Put32(rom, 0x100 + i * 4, handler[i]);
    Bus bus(std::move(rom));
    Cpu cpu(bus, true);
    Ppu ppu(bus);
    cpu.SetCpsr(0xA000003Fu);
    cpu.SetRegister(0, 0x1234);
    cpu.SetRegister(1, 0x5678);
    bus.Write32(0x03007FFCu, 0x08000100u);
    bus.Write16(0x04000200u, 1);
    bus.Write16(0x04000004u, 8);
    cpu.Step();
    REQUIRE(cpu.IsHalted());
    REQUIRE(bus.Read16(0x04000208u) == 1);
    ppu.Advance(160 * 1232);
    REQUIRE(bus.Read16(0x04000202u) == 1);
    cpu.Step();
    REQUIRE(cpu.GetRegister(15) == 0x08000100u);
    REQUIRE((cpu.GetCpsr() & 0xBFu) == 0x92u);
    for (int step = 0; step < 9; ++step) cpu.Step();
    REQUIRE(cpu.GetRegister(15) == 0x08000002u);
    REQUIRE(cpu.GetCpsr() == 0xA000003Fu);
    REQUIRE(cpu.GetRegister(0) == 0x1234);
    REQUIRE(cpu.GetRegister(1) == 0x5678);
    REQUIRE(cpu.GetRegister(13) == 0x03007F00u);
    REQUIRE_FALSE(cpu.IsHalted());
    REQUIRE(bus.Read16(0x03007FF8u) == 0);
    REQUIRE(bus.Read16(0x04000202u) == 0);
    cpu.Step();
    REQUIRE(cpu.GetRegister(2) == 42);
}

TEST_CASE("Halt wakes on enabled pending interrupt even when IME is zero") {
    Bus bus(cpu_test::AsRom(0x222ADF02u));
    Cpu cpu(bus, true);
    cpu.SetCpsr(0x3F);
    cpu.Step();
    REQUIRE(cpu.IsHalted());
    const u32 pc = cpu.GetRegister(15);
    cpu.Step();
    REQUIRE(cpu.GetRegister(15) == pc);
    bus.Write16(0x04000200u, 1);
    bus.RequestInterrupt(1);
    cpu.Step();
    REQUIRE_FALSE(cpu.IsHalted());
    REQUIRE(cpu.GetRegister(2) == 42);
}

TEST_CASE("ARM BIOS calls select the service from the upper immediate byte") {
    Bus bus(cpu_test::AsRom(0xEF010000u));
    Cpu cpu(bus, true);
    cpu.SetRegister(0, 2);
    bus.Write32(0x03007DFCu, 0xFFFFFFFF);
    bus.Write32(0x03007E00u, 0x12345678);
    cpu.Step();
    REQUIRE(bus.Read32(0x03007DFCu) == 0);
    REQUIRE(bus.Read32(0x03007E00u) == 0x12345678);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("CpuSet and CpuFastSet implement copy fill width and block rounding") {
    for (bool fast : {false, true}) {
        Bus bus(cpu_test::AsRom(fast ? 0xDF0Cu : 0xDF0Bu));
        Cpu cpu(bus, true);
        cpu.SetCpsr(0x3F);
        cpu.SetRegister(0, 0x02000000);
        cpu.SetRegister(1, 0x03000000);
        cpu.SetRegister(2, fast ? 9u : 3u);
        for (u32 i = 0; i < 32; ++i) bus.Write16(0x02000000 + i * 2, static_cast<u16>(i + 1));
        cpu.Step();
        REQUIRE(bus.Read16(0x03000004) == 3);
        REQUIRE(bus.Read16(0x03000006) == (fast ? 4 : 0));
        if (fast) REQUIRE(bus.Read16(0x0300003E) == 32);
        cpu.SetRegister(15, 0x08000000);
        cpu.SetRegister(2, (1u << 24) | (1u << 26) | 1u);
        bus.Write32(0x02000000, 0x87654321);
        cpu.Step();
        REQUIRE(bus.Read32(0x03000000) == 0x87654321);
        if (fast) REQUIRE(bus.Read32(0x0300001C) == 0x87654321);
    }
}

TEST_CASE("BIOS division handles signed remainder and INT_MIN without overflow") {
    Bus bus(cpu_test::AsRom(0xDF06u));
    Cpu cpu(bus, true);
    cpu.SetCpsr(0x3F);
    cpu.SetRegister(0, static_cast<u32>(-7));
    cpu.SetRegister(1, 3);
    cpu.Step();
    REQUIRE(cpu.GetRegister(0) == static_cast<u32>(-2));
    REQUIRE(cpu.GetRegister(1) == static_cast<u32>(-1));
    REQUIRE(cpu.GetRegister(3) == 2);
    cpu.SetRegister(15, 0x08000000);
    cpu.SetRegister(0, 0x80000000);
    cpu.SetRegister(1, 0xFFFFFFFF);
    cpu.Step();
    REQUIRE(cpu.GetRegister(0) == 0x80000000);
    REQUIRE(cpu.GetRegister(1) == 0);
}

TEST_CASE("BIOS Sqrt returns the integer root and resumes after SWI 08") {
    for (bool thumb : {true, false}) {
        Bus bus(cpu_test::AsRom(thumb ? 0xDF08u : 0xEF080000u));
        Cpu cpu(bus, true);
        cpu.SetCpsr(thumb ? 0x3Fu : 0x1Fu);
        for (const auto [input, expected] : {
                 std::pair<u32, u32>{0u, 0u}, {2u, 1u}, {15u, 3u},
                 {16u, 4u}, {0x001E4000u, 1408u}, {0xFFFFFFFFu, 65535u}}) {
            cpu.SetRegister(15, 0x08000000u);
            cpu.SetRegister(0, input);
            cpu.SetRegister(1, 0x12345678u);
            cpu.Step();
            REQUIRE(cpu.GetRegister(0) == expected);
            REQUIRE(cpu.GetRegister(1) == 0x12345678u);
            REQUIRE(cpu.GetRegister(15) == 0x08000000u + (thumb ? 2u : 4u));
        }
    }
}

TEST_CASE("BgAffineSet writes signed matrix and translated reference coordinates") {
    Bus bus(cpu_test::AsRom(0xDF0Eu));
    Cpu cpu(bus, true);
    cpu.SetCpsr(0x3F);
    cpu.SetRegister(0, 0x02000000);
    cpu.SetRegister(1, 0x03000000);
    cpu.SetRegister(2, 1);
    bus.Write32(0x02000000, 20 * 256);
    bus.Write32(0x02000004, 30 * 256);
    bus.Write16(0x02000008, 3);
    bus.Write16(0x0200000A, 4);
    bus.Write16(0x0200000C, 0x100);
    bus.Write16(0x0200000E, 0x200);
    bus.Write16(0x02000010, 0x4000); // 90 degrees.
    cpu.Step();
    REQUIRE(bus.Read16(0x03000000) == 0);
    REQUIRE(bus.Read16(0x03000002) == 0xFF00);
    REQUIRE(bus.Read16(0x03000004) == 0x200);
    REQUIRE(bus.Read16(0x03000006) == 0);
    REQUIRE(bus.Read32(0x03000008) == 24 * 256);
    REQUIRE(bus.Read32(0x0300000C) == 24 * 256);
}

TEST_CASE("Supplied BIOS retains exception entry even when BIOS emulation is enabled") {
    Bus bus(cpu_test::AsRom(0xDF01u), std::vector<u8>(kBiosSize));
    Cpu cpu(bus, true);
    cpu.SetCpsr(0x3F);
    cpu.Step();
    REQUIRE(cpu.GetRegister(15) == 8);
    REQUIRE((cpu.GetCpsr() & 0x3Fu) == 0x13u);
}

TEST_CASE("Hardware IRQ enters vector 18 and SUBS restores ARM or Thumb execution") {
    for (bool thumb : {false, true}) {
        std::vector<u8> bios(kBiosSize);
        cpu_test::Put32(bios, 0x18, 0xE25EF004u); // SUBS pc,lr,4
        Bus bus(cpu_test::AsRom(0xE1A00000u), std::move(bios));
        Cpu cpu(bus);
        const u32 original = 0xA000001Fu | (thumb ? kThumbFlag : 0);
        cpu.SetCpsr(original);
        bus.Write16(0x04000200, 1);
        bus.Write16(0x04000208, 1);
        bus.RequestInterrupt(1);
        cpu.Step();
        REQUIRE(cpu.GetRegister(15) == 0x18);
        REQUIRE(cpu.GetRegister(14) == 0x08000004);
        REQUIRE((cpu.GetCpsr() & 0xBFu) == 0x92u);
        bus.Write16(0x04000202, 1);
        cpu.Step();
        REQUIRE(cpu.GetRegister(15) == 0x08000000);
        REQUIRE(cpu.GetCpsr() == original);
        REQUIRE(cpu.GetRegister(13) == 0x03007F00);
    }
}
