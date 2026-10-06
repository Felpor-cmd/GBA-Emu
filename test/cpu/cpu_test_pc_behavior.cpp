#include <catch2/catch_test_macros.hpp>

#include "cpu_test_helpers.hpp"

TEST_CASE("ARM data processing reads PC as instruction_address + 8") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x4, true, 15, 0, 0)));
    Cpu cpu(bus);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x08000008);
    REQUIRE((cpu.GetCpsr() & (1u << 30)) == 0);
}

TEST_CASE("ARM data processing reads PC in register operand") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x4, true, 0, 0, 15)));
    Cpu cpu(bus);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x08000008);
}

TEST_CASE("ARM data processing uses PC in shifted register operand") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeShiftedRegister(0xE, 0x4, true, 0, 0, 0, 0, 15)));
    Cpu cpu(bus);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x08000008);
}

TEST_CASE("ARM data processing reads PC in register-specified shift amount") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeShiftedRegister(0xE, 0x4, true, 0, 0, 1, 0, 15)));
    Cpu cpu(bus);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x10000010);
}

TEST_CASE("ARM data processing writes to PC (MOV pc, lr)") {
    u32 instr = cpu_test::Encode(0xE, 0xD, false, 0, 15, 14);
    Bus bus(cpu_test::AsRom(instr));
    Cpu cpu(bus);
    cpu.SetRegister(14, 0x02000000);
    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x02000000);
}

TEST_CASE("ARM data processing writes to PC with alignment") {
    u32 instr = cpu_test::Encode(0xE, 0xD, false, 0, 15, 14);
    Bus bus(cpu_test::AsRom(instr));
    Cpu cpu(bus);
    cpu.SetRegister(14, 0x02000003);
    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x02000000);
}

TEST_CASE("ARM data processing with S bit and Rd=PC restores CPSR from SPSR") {
    u32 instr = cpu_test::Encode(0xE, 0xD, true, 0, 15, 14);
    Bus bus(cpu_test::AsRom(instr));
    Cpu cpu(bus);
    cpu.SetRegister(14, 0x02000000);
    cpu.SetCpsr(0x1B);  // Undefined mode
    cpu.SetSpsr(0x1F);  // System mode, ARM state
    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x02000000);
    REQUIRE((cpu.GetCpsr() & 0x1F) == 0x1F);
}

TEST_CASE("ARM LDR with PC destination loads value and updates PC") {
    std::vector<u8> rom(0x1000, 0);
    cpu_test::Put32(rom, 0, cpu_test::EncodeSingleDataTransfer(0xE, true, false, 15, 15, 0));
    cpu_test::Put32(rom, 8, 0x02000000);
    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x02000000);
}

TEST_CASE("ARM LDR with PC destination aligns PC") {
    std::vector<u8> rom(0x1000, 0);
    cpu_test::Put32(rom, 0, cpu_test::EncodeSingleDataTransfer(0xE, true, false, 15, 15, 0));
    cpu_test::Put32(rom, 8, 0x02000003);
    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x02000000);
}

TEST_CASE("ARM LDM with PC in register list loads PC") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(0xE, true, 13, (1u << 15), true, true, false, false)));
    Cpu cpu(bus);
    cpu.SetRegister(13, 0x03007F00);
    bus.Write32(0x03007F04, 0x02000000);
    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x02000000);
}

TEST_CASE("ARM LDM with PC in register list aligns PC") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(0xE, true, 13, (1u << 15), true, true, false, false)));
    Cpu cpu(bus);
    cpu.SetRegister(13, 0x03007F00);
    bus.Write32(0x03007F04, 0x02000003);
    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x02000000);
}

TEST_CASE("ARM LDM with S bit and PC restores CPSR from SPSR") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeBlockDataTransfer(0xE, true, 13, (1u << 15), true, true, false, true)));
    Cpu cpu(bus);
    cpu.SetRegister(13, 0x03007F00);
    bus.Write32(0x03007F04, 0x02000000);
    cpu.SetCpsr(0x1B);  // Undefined mode
    cpu.SetSpsr(0x1F | 0x20);  // System mode, Thumb state
    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x02000000);
    REQUIRE((cpu.GetCpsr() & 0x1F) == 0x1F);
    REQUIRE((cpu.GetCpsr() & 0x20) != 0);
}

TEST_CASE("ARM single data transfer reads PC as base register") {
    std::vector<u8> rom(0x1000, 0);
    cpu_test::Put32(rom, 0, cpu_test::EncodeSingleDataTransfer(0xE, true, false, 15, 0, 4));
    cpu_test::Put32(rom, 0xC, 0x12345678);
    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x12345678);
}

TEST_CASE("ARM single data transfer reads PC as offset register") {
    std::vector<u8> rom(0x1000, 0);
    cpu_test::Put32(rom, 0, cpu_test::EncodeRegisterDataTransfer(0xE, true, false, 1, 0, 15, 0, 0));
    cpu_test::Put32(rom, 0x8, 0x12345678);
    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x02000000);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x12345678);
}

TEST_CASE("ARM block data transfer reads PC as base register") {
    std::vector<u8> rom(0x1000, 0);
    cpu_test::Put32(rom, 0, cpu_test::EncodeBlockDataTransfer(0xE, true, 15, 1, true, true));
    cpu_test::Put32(rom, 0xC, 0x12345678);
    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x12345678);
}

TEST_CASE("ARM data processing PC write does not modify flags when S=0") {
    u32 instr = cpu_test::Encode(0xE, 0xD, false, 0, 15, 14);
    Bus bus(cpu_test::AsRom(instr));
    Cpu cpu(bus);
    cpu.SetRegister(14, 0x02000000);
    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x02000000);
    REQUIRE(cpu.GetCpsr() == 0x1F);
}

TEST_CASE("ARM data processing with S=1 and Rd=PC restores CPSR and mode") {
    u32 instr = cpu_test::Encode(0xE, 0xD, true, 15, 15, 14);
    Bus bus(cpu_test::AsRom(instr));
    Cpu cpu(bus);
    cpu.SetRegister(14, 0x02000000);
    cpu.SetRegister(15, 0x08000000);
    cpu.SetCpsr(0x13 | 0x80);  // SVC mode, IRQ disabled
    cpu.SetSpsr(0x10);  // User mode
    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x02000000);
    REQUIRE((cpu.GetCpsr() & 0x1F) == 0x10);
}