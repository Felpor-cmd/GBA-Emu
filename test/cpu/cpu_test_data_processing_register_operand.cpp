#include <catch2/catch_test_macros.hpp>

#include "cpu_test_helpers.hpp"

TEST_CASE("Cpu::Step ADD sets r0 and clears flags") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x4, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 5);
    cpu.SetRegister(2, 3);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 8);
    REQUIRE(((cpu.GetCpsr() >> 30) & 1) == 0);
    REQUIRE(((cpu.GetCpsr() >> 29) & 1) == 0);
}

TEST_CASE("Cpu::Step CMP leaves r1 unchanged and sets flags") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0xA, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 5);
    cpu.SetRegister(2, 3);
    cpu.Step();

    REQUIRE(cpu.GetRegister(1) == 5);
    REQUIRE(((cpu.GetCpsr() >> 30) & 1) == 0);
}

TEST_CASE("Cpu::Step ADC adds with carry in") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x5, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 5);
    cpu.SetRegister(2, 3);
    cpu.SetCpsr(cpu.GetCpsr() | (1u << 29));  // Set C flag
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 9);
}

TEST_CASE("Cpu::Step ADC sets carry on unsigned overflow") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x5, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0xFFFFFFFF);  // Max unsigned
    cpu.SetRegister(2, 1);
    cpu.SetCpsr(cpu.GetCpsr() | (1u << 29));  // Set C flag
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 1);  // 0xFFFFFFFF + 1 + 1 = 0x100000001 -> 1 (with carry)
    REQUIRE((cpu.GetCpsr() & (1u << 29)) != 0);  // Carry set (unsigned overflow)
    REQUIRE((cpu.GetCpsr() & (1u << 28)) == 0);  // Overflow clear (different signs)
}

TEST_CASE("Cpu::Step ADC sets overflow on signed overflow") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x5, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x7FFFFFFF);  // Max positive signed
    cpu.SetRegister(2, 1);
    cpu.SetCpsr(cpu.GetCpsr() & ~(1u << 29));  // Clear C flag
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x80000000);  // 0x7FFFFFFF + 1 = 0x80000000
    REQUIRE((cpu.GetCpsr() & (1u << 29)) == 0);  // Carry clear (no unsigned overflow)
    REQUIRE((cpu.GetCpsr() & (1u << 28)) != 0);  // Overflow set (positive + positive = negative)
}

TEST_CASE("Cpu::Step SBC subtracts with carry in") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x6, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 10);
    cpu.SetRegister(2, 3);
    cpu.SetCpsr(cpu.GetCpsr() | (1u << 29));  // Set C flag (carry = 1 means no borrow)
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 7);  // 10 - 3 = 7 (C=1 means no borrow)
}

TEST_CASE("Cpu::Step SBC subtracts with borrow when carry clear") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x6, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 10);
    cpu.SetRegister(2, 3);
    cpu.SetCpsr(cpu.GetCpsr() & ~(1u << 29));  // Clear C flag (carry = 0 means borrow=1)
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 6);  // 10 - 3 - 1 = 6
}

TEST_CASE("Cpu::Step SBC sets carry on borrow") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x6, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 5);
    cpu.SetRegister(2, 10);
    cpu.SetCpsr(cpu.GetCpsr() & ~(1u << 29));  // Clear C
    cpu.Step();

    REQUIRE((cpu.GetCpsr() & (1u << 29)) == 0);  // Carry clear (borrow)
}

TEST_CASE("Cpu::Step RSC reverse subtracts with carry") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x7, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 5);
    cpu.SetRegister(2, 10);
    cpu.SetCpsr(cpu.GetCpsr() | (1u << 29));  // Set C flag
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 5);  // 10 - 5 - 0 = 5
}

TEST_CASE("Cpu::Step RSC reverse subtracts with borrow when carry clear") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x7, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 5);
    cpu.SetRegister(2, 10);
    cpu.SetCpsr(cpu.GetCpsr() & ~(1u << 29));  // Clear C
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 4);  // 10 - 5 - 1 = 4
}

TEST_CASE("Cpu::Step AND sets N and Z flags") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x0, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0xFF);
    cpu.SetRegister(2, 0x0F);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0x0F);
    REQUIRE((cpu.GetCpsr() & (1u << 31)) == 0);  // N clear
    REQUIRE((cpu.GetCpsr() & (1u << 30)) == 0);  // Z clear
}

TEST_CASE("Cpu::Step EOR sets N and Z flags") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x1, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0xFF);
    cpu.SetRegister(2, 0xFF);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0);
    REQUIRE((cpu.GetCpsr() & (1u << 30)) != 0);  // Z set
}

TEST_CASE("Cpu::Step SUB sets carry and overflow") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x2, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 5);
    cpu.SetRegister(2, 10);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == (5u - 10u));
    REQUIRE((cpu.GetCpsr() & (1u << 29)) == 0);  // Carry clear (borrow)
}

TEST_CASE("Cpu::Step RSB reverse subtracts") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x3, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 10);
    cpu.SetRegister(2, 5);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xFFFFFFFBu);  // RSB: op2 - op1 = 5 - 10 = -5
}

TEST_CASE("Cpu::Step ADD sets overflow on signed overflow") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x4, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x7FFFFFFF);
    cpu.SetRegister(2, 1);
    cpu.Step();

    REQUIRE((cpu.GetCpsr() & (1u << 28)) != 0);  // Overflow set
}

TEST_CASE("Cpu::Step ORR sets N and Z flags") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0xC, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0x80000000);  // Has sign bit set
    cpu.SetRegister(2, 0x7FFFFFFF);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xFFFFFFFF);
    REQUIRE((cpu.GetCpsr() & (1u << 31)) != 0);  // N set
    REQUIRE((cpu.GetCpsr() & (1u << 30)) == 0);  // Z clear
}

TEST_CASE("Cpu::Step MOV copies register") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0xD, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(2, 0xDEADBEEF);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEF);
}

TEST_CASE("Cpu::Step BIC clears bits") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0xE, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0xFF);
    cpu.SetRegister(2, 0x0F);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xF0);
}

TEST_CASE("Cpu::Step MVN inverts operand") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0xF, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(2, 0x000000FF);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xFFFFFF00);
}

TEST_CASE("Cpu::Step TST only sets flags") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x8, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0xFF);
    cpu.SetRegister(2, 0x0F);
    cpu.SetRegister(0, 0xDEADBEEF);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEF);  // Unchanged
    REQUIRE((cpu.GetCpsr() & (1u << 31)) == 0);  // N clear (result != 0, but sign bit 0)
    REQUIRE((cpu.GetCpsr() & (1u << 30)) == 0);  // Z clear
}

TEST_CASE("Cpu::Step TEQ only sets flags") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0x9, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 0xFF);
    cpu.SetRegister(2, 0xFF);
    cpu.SetRegister(0, 0xDEADBEEF);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEF);  // Unchanged
    REQUIRE((cpu.GetCpsr() & (1u << 30)) != 0);  // Z set
}

TEST_CASE("Cpu::Step CMP only sets flags") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0xA, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 10);
    cpu.SetRegister(2, 5);
    cpu.SetRegister(0, 0xDEADBEEF);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEF);  // Unchanged
    REQUIRE((cpu.GetCpsr() & (1u << 30)) == 0);  // Z clear
    REQUIRE((cpu.GetCpsr() & (1u << 29)) != 0);  // Carry set (no borrow)
}

TEST_CASE("Cpu::Step CMN only sets flags") {
    Bus bus(cpu_test::AsRom(cpu_test::Encode(0xE, 0xB, true, 1, 0, 2)));
    Cpu cpu(bus);
    cpu.SetRegister(1, 5);
    cpu.SetRegister(2, 5);
    cpu.SetRegister(0, 0xDEADBEEF);
    cpu.Step();

    REQUIRE(cpu.GetRegister(0) == 0xDEADBEEF);  // Unchanged
    REQUIRE((cpu.GetCpsr() & (1u << 30)) == 0);  // Z clear (5 + 5 = 10 != 0)
    REQUIRE((cpu.GetCpsr() & (1u << 29)) == 0);  // Carry clear (no unsigned overflow)
}
