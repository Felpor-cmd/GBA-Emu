#include <catch2/catch_test_macros.hpp>

#include "cpu_test_helpers.hpp"

TEST_CASE("Cpu::Step applies LSL to a register operand") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeShiftedRegister(0xE, 0xD, false,
                                                            0, 0, 2, 0, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 3);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 12);
}

TEST_CASE("Cpu::Step applies LSR to a register operand") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeShiftedRegister(0xE, 0xD, false,
                                                            0, 0, 2, 1, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 16);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 4);
}

TEST_CASE("Cpu::Step applies ASR to a register operand") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeShiftedRegister(0xE, 0xD, false,
                                                            0, 0, 2, 2, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x80000008);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xE0000002);
}

TEST_CASE("Cpu::Step applies ROR to a register operand") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeShiftedRegister(0xE, 0xD, false,
                                                            0, 0, 4, 3, 1)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x80000001);

    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x18000000);
}

TEST_CASE("ARM register-specified zero shift preserves the operand and carry") {
    for (u32 type = 0; type < 4; ++type) {
        const u32 instruction = 0xE1B00110u | (type << 5); // MOVS r0,r0,<shift> r1
        Bus bus(cpu_test::AsRom(instruction));
        Cpu cpu(bus);
        cpu.SetCpsr(0x1Fu | (1u << 29));
        cpu.SetRegister(0, 1);
        cpu.SetRegister(1, 0);
        cpu.Step();
        REQUIRE(cpu.GetRegister(0) == 1u);
        REQUIRE((cpu.GetCpsr() & (1u << 29)) != 0);
    }
}

TEST_CASE("ARM register-specified shift reads PC plus twelve") {
    for (u32 instruction : {0xE1A0001Fu, 0xE08F0010u}) {
        Bus bus(cpu_test::AsRom(instruction));
        Cpu cpu(bus);
        cpu.SetRegister(0, 0);
        cpu.Step();
        REQUIRE(cpu.GetRegister(0) == 0x0800000Cu);
    }
}
