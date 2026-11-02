#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <cstdlib>
#include <fstream>

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

TEST_CASE("Thumb RegisterRamReset clears selected RAM and resumes Thumb code") {
    Bus bus(cpu_test::AsRom(0xDF01u));
    Cpu cpu(bus, true);
    REQUIRE(cpu.SetCpsr(0x3Fu));
    cpu.SetRegister(0, 1u << 0);
    cpu.SetRegister(1, 0x12345678u);
    bus.Write32(0x02000000u, 0xDEADBEEFu);
    bus.Write32(0x03000000u, 0xCAFEBABEu);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000000u) == 0);
    REQUIRE(bus.Read32(0x03000000u) == 0xCAFEBABEu);
    REQUIRE(cpu.GetRegister(1) == 0x12345678u);
    REQUIRE((cpu.GetCpsr() & kThumbFlag) != 0);
    REQUIRE(cpu.GetRegister(15) == 0x08000002u);
}

TEST_CASE("RegisterRamReset preserves the BIOS stack area and nested return addresses") {
    // PUSH {lr}; SWI 1; POP {pc}, with Zelda's reset flags and stack location.
    std::vector<u8> rom(8, 0);
    cpu_test::Put32(rom, 0, 0xDF01B500u);
    cpu_test::Put32(rom, 4, 0x0000BD00u);
    Bus bus(std::move(rom));
    Cpu cpu(bus, true);
    cpu.SetCpsr(0x3Fu);
    cpu.SetRegister(0, 0xFEu);
    cpu.SetRegister(13, 0x03007EF0u);
    cpu.SetRegister(14, 0x08000101u);
    bus.Write32(0x03007DFCu, 0xDEADBEEFu);
    bus.Write32(0x03007E00u, 0x12345678u);
    bus.Write32(0x03007FFCu, 0x87654321u);
    cpu.Step();
    cpu.Step();
    REQUIRE(bus.Read32(0x03007DFCu) == 0);
    REQUIRE(bus.Read32(0x03007E00u) == 0x12345678u);
    REQUIRE(bus.Read32(0x03007FFCu) == 0x87654321u);
    REQUIRE(bus.Read32(0x03007EECu) == 0x08000101u);
    REQUIRE(bus.Read16(0x04000000u) == 0x0080u);
    cpu.Step();
    REQUIRE(cpu.GetRegister(15) == 0x08000100u);
    REQUIRE(cpu.GetRegister(13) == 0x03007EF0u);
}

TEST_CASE("Conditional ARM SWI has no exception side effects when condition fails") {
    Bus bus(cpu_test::AsRom(0x0F000000u));
    Cpu cpu(bus);
    const u32 cpsr_before = cpu.GetCpsr();

    cpu.Step();

    REQUIRE(cpu.GetCpsr() == cpsr_before);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("BIOS-free SWI 12 expands literals through VRAM halfword writes") {
    Bus bus(cpu_test::AsRom(0xDF12u));
    Cpu cpu(bus, true);
    cpu.SetCpsr(0x3Fu);
    cpu.SetRegister(0, 0x02000000u);
    cpu.SetRegister(1, 0x06000000u);
    cpu.SetRegister(2, 0x12345678u);
    cpu.SetRegister(3, 0x9ABCDEF0u);
    bus.Write32(0x02000000u, 0x00000810u);
    bus.Write8(0x02000004u, 0);
    for (u32 i = 0; i < 8; ++i) bus.Write8(0x02000005u + i, static_cast<u8>('A' + i));
    const u32 cycles = cpu.Step();
    REQUIRE(cycles == 50); // ROM fetch/refill, EWRAM source reads, four VRAM writes.
    for (u32 i = 0; i < 4; ++i)
        REQUIRE(bus.Read16(0x06000000u + 2 * i) == static_cast<u16>('A' + 2 * i + (('B' + 2 * i) << 8)));
    REQUIRE(cpu.GetRegister(0) == 0x0200000Du);
    REQUIRE(cpu.GetRegister(1) == 0x06000008u);
    REQUIRE(cpu.GetRegister(2) == 0x12345678u);
    REQUIRE(cpu.GetRegister(3) == 0);
    REQUIRE(cpu.GetRegister(15) == 0x08000002u);
}

TEST_CASE("BIOS-free SWI 12 expands overlapping backreferences") {
    Bus bus(cpu_test::AsRom(0xDF12u));
    Cpu cpu(bus, true);
    cpu.SetCpsr(0x3Fu);
    cpu.SetRegister(0, 0x02000000u);
    cpu.SetRegister(1, 0x06000000u);
    bus.Write32(0x02000000u, 0x00000810u);
    bus.Write8(0x02000004u, 0x20u); // Two literals, then six bytes at distance 2.
    bus.Write8(0x02000005u, 'A');
    bus.Write8(0x02000006u, 'B');
    bus.Write8(0x02000007u, 0x30u);
    bus.Write8(0x02000008u, 0x01u);
    const u32 cycles = cpu.Step();
    REQUIRE(cycles == 38); // One token replaces six literal source reads.
    for (u32 i = 0; i < 4; ++i) REQUIRE(bus.Read16(0x06000000u + 2 * i) == 0x4241u);
}

TEST_CASE("Local BIOS SWI 12 returns the observed registers and VRAM bytes") {
    const char* path = std::getenv("GBA_TEST_BIOS");
    if (!path) {
        SUCCEED("Set GBA_TEST_BIOS for local real-BIOS comparison");
        return;
    }
    std::ifstream file(path, std::ios::binary);
    std::vector<u8> bios((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Bus bus(cpu_test::AsRom(0xDF12u), std::move(bios));
    Cpu cpu(bus, true);
    cpu.SetCpsr(0x3Fu);
    cpu.SetRegister(0, 0x02000000u);
    cpu.SetRegister(1, 0x06000000u);
    cpu.SetRegister(2, 0x12345678u);
    cpu.SetRegister(3, 0x9ABCDEF0u);
    bus.Write32(0x02000000u, 0x00000810u);
    bus.Write8(0x02000004u, 0);
    for (u32 i = 0; i < 8; ++i) bus.Write8(0x02000005u + i, static_cast<u8>('A' + i));
    for (int i = 0; i < 5000 && !(i && cpu.GetRegister(15) == 0x08000002u); ++i) cpu.Step();
    REQUIRE(cpu.GetRegister(15) == 0x08000002u);
    REQUIRE(cpu.GetRegister(0) == 0x0200000Du);
    REQUIRE(cpu.GetRegister(1) == 0x06000008u);
    REQUIRE(cpu.GetRegister(2) == 0x12345678u);
    REQUIRE(cpu.GetRegister(3) == 0);
    REQUIRE(cpu.GetCycles() == 310);
    for (u32 i = 0; i < 4; ++i)
        REQUIRE(bus.Read16(0x06000000u + 2 * i) == static_cast<u16>('A' + 2 * i + (('B' + 2 * i) << 8)));
}

TEST_CASE("BIOS-free SWI 11 writes decompressed bytes to work RAM") {
    Bus bus(cpu_test::AsRom(0xDF11u));
    Cpu cpu(bus, true);
    cpu.SetCpsr(0x3Fu);
    cpu.SetRegister(0, 0x02000000u);
    cpu.SetRegister(1, 0x03000001u);
    bus.Write32(0x02000000u, 0x00000810u);
    bus.Write8(0x02000004u, 0x20u);
    bus.Write8(0x02000005u, 'A');
    bus.Write8(0x02000006u, 'B');
    bus.Write8(0x02000007u, 0x30u);
    bus.Write8(0x02000008u, 0x01u);
    REQUIRE(cpu.Step() > 38);
    REQUIRE(bus.Read8(0x03000000u) == 0);
    for (u32 i = 0; i < 8; ++i) REQUIRE(bus.Read8(0x03000001u + i) == (i & 1 ? 'B' : 'A'));
    REQUIRE(cpu.GetRegister(0) == 0x02000009u);
    REQUIRE(cpu.GetRegister(1) == 0x03000009u);
}
