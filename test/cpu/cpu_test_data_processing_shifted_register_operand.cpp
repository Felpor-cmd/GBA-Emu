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
