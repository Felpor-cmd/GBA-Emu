#include <catch2/catch_test_macros.hpp>

#include "cpu_test_helpers.hpp"

TEST_CASE("ARM LDR unaligned word load rotates according to address bits [1:0]") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, true, false, 1, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write32(0x02000000, 0x11223344);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x44112233);
}

TEST_CASE("ARM LDR unaligned word load at address +2 rotates by 16 bits") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, true, false, 1, 2, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write32(0x02000000, 0x11223344);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x33441122);
}

TEST_CASE("ARM LDR unaligned word load at address +3 rotates by 24 bits") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, true, false, 1, 2, 3)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write32(0x02000000, 0x11223344);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x22334411);
}

TEST_CASE("ARM LDR aligned word load does not rotate") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, true, false, 1, 2, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000004);
    bus.Write32(0x02000004, 0x11223344);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x11223344);
}

TEST_CASE("ARM STR unaligned word store writes to aligned address") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, false, false, 1, 0, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x11223344);
    cpu.SetRegister(1, 0x02000001);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000000) == 0x11223344);
    REQUIRE(bus.Read32(0x02000004) == 0);
}

TEST_CASE("ARM LDRH at odd address loads from aligned address") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeHalfwordDataTransfer(0xE, true, false, true, 1, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write16(0x02000000, 0xABCD);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x0000ABCD);
}

TEST_CASE("ARM STRH at odd address stores to aligned address") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeHalfwordDataTransfer(0xE, false, false, true, 1, 0, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0xABCD);
    cpu.SetRegister(1, 0x02000000);

    cpu.Step();

    REQUIRE(bus.Read16(0x02000000) == 0xABCD);
    REQUIRE(bus.Read16(0x02000002) == 0);
}

TEST_CASE("ARM LDRSH at odd address loads from aligned address and sign-extends") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeHalfwordDataTransfer(0xE, true, true, true, 1, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write16(0x02000000, 0x8000);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0xFFFF8000);
}

TEST_CASE("ARM LDRB at odd address works normally") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, true, true, 1, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write8(0x02000001, 0xAB);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x000000AB);
}

TEST_CASE("ARM STRB at odd address works normally") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, false, true, 1, 0, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x123456AB);
    cpu.SetRegister(1, 0x02000000);

    cpu.Step();

    REQUIRE(bus.Read8(0x02000001) == 0xAB);
}

TEST_CASE("ARM LDR with Rn == Rd and writeback loads value then updates base") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, true, false, 1, 1, 4, true, true)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write32(0x02000004, 0xDEADBEEF);

    cpu.Step();

    REQUIRE(cpu.GetRegister(1) == 0xDEADBEEF);
}

TEST_CASE("ARM STR with Rn == Rd and writeback stores old value") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, false, false, 1, 1, 4, true, true)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000004) == 0x02000000);
    REQUIRE(cpu.GetRegister(1) == 0x02000004);
}

TEST_CASE("ARM LDR with base register and offset register overlap") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeRegisterDataTransfer(0xE, true, false, 1, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);  // Rn=Rm=1, base=0x02000000, offset=r1=0x02000000
    bus.Write32(0x04000000, 0x12345678);  // effective = base + offset = 0x02000000 + 0x02000000 = 0x04000000

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x12345678);
}

TEST_CASE("ARM STR with base register and offset register overlap") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeRegisterDataTransfer(0xE, false, false, 1, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);  // Rn=Rm=1
    cpu.SetRegister(2, 0xDEADBEEF);

    cpu.Step();

    REQUIRE(bus.Read32(0x04000000) == 0xDEADBEEF);
}

TEST_CASE("ARM LDR with PC as base register") {
    std::vector<u8> rom(0x1000, 0);
    cpu_test::Put32(rom, 0, cpu_test::EncodeSingleDataTransfer(0xE, true, false, 15, 2, 4));
    cpu_test::Put32(rom, 0xC, 0xCAFED00D);
    Bus bus(std::move(rom));
    Cpu cpu(bus);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0xCAFED00D);
}

TEST_CASE("ARM STR with PC as base register") {
    std::vector<u8> rom(0x1000, 0);
    cpu_test::Put32(rom, 0, cpu_test::EncodeSingleDataTransfer(0xE, false, false, 15, 0, 4));
    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, 0x12345678);

    cpu.Step();

    // PC as base: PC=0x08000000, GetRegisterWithPC(15)=0x08000008, offset=4 -> 0x0800000C
    // But 0x0800000C is ROM (read-only), so write is ignored on GBA
    // Test that the instruction executes without crashing
    REQUIRE(cpu.GetRegister(15) == 0x08000004);
}

TEST_CASE("ARM LDR with PC as source register (Rd=15)") {
    std::vector<u8> rom(0x1000, 0);
    cpu_test::Put32(rom, 0, cpu_test::EncodeSingleDataTransfer(0xE, true, false, 15, 15, 4));
    cpu_test::Put32(rom, 0xC, 0x02000000);
    Bus bus(std::move(rom));
    Cpu cpu(bus);

    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x02000000);
}

TEST_CASE("ARM LDR with PC as offset register") {
    std::vector<u8> rom(0x1000, 0);
    cpu_test::Put32(rom, 0, cpu_test::EncodeRegisterDataTransfer(0xE, true, false, 1, 2, 15, 0, 0));
    cpu_test::Put32(rom, 8, 0x12345678);
    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0x12345678);
}

TEST_CASE("ARM LDM unaligned word loads rotate") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(0xE, true, 13, (1u << 0), true, true, false, false)));
    Cpu cpu(bus);
    cpu.SetRegister(13, 0x02000001);  // unaligned base
    bus.Write32(0x02000004, 0x11223344);  // data at aligned address

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x44112233);
}

TEST_CASE("ARM LDRSB at odd address works normally") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeHalfwordDataTransfer(0xE, true, true, false, 1, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write8(0x02000001, 0x80);

    cpu.Step();

    REQUIRE(cpu.GetRegister(2) == 0xFFFFFF80);
}

TEST_CASE("ARM LDR with post-indexed writeback and Rn == Rd") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, true, false, 1, 1, 4, false)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    bus.Write32(0x02000000, 0x11223344);

    cpu.Step();

    REQUIRE(cpu.GetRegister(1) == 0x11223344);
    REQUIRE(cpu.GetRegister(15) == 0x08000004);
}

TEST_CASE("ARM STR with post-indexed writeback and Rn == Rd") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSingleDataTransfer(0xE, false, false, 1, 1, 4, false)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);

    cpu.Step();

    REQUIRE(bus.Read32(0x02000000) == 0x02000000);
    REQUIRE(cpu.GetRegister(1) == 0x02000004);
}