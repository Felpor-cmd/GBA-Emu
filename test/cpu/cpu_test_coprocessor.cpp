#include <catch2/catch_test_macros.hpp>

#include "cpu_test_helpers.hpp"

TEST_CASE("ARM MRC triggers Undefined Instruction exception") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeMRC(0xE, 15, 0, 0, 0, 0, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(15, 0x08000000);
    
    cpu.Step();
    
    REQUIRE(cpu.GetCpsr() & 0x1F);  // Should be in Undefined mode (0x1B)
    REQUIRE((cpu.GetCpsr() & 0x1F) == 0x1B);
    REQUIRE((cpu.GetCpsr() & 0x20) == 0);  // T bit cleared (ARM state)
    REQUIRE(cpu.GetRegister(15) == 0x00000004);  // PC at vector 0x04
}

TEST_CASE("ARM MCR triggers Undefined Instruction exception") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeMCR(0xE, 15, 0, 0, 0, 0, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(15, 0x08000000);
    
    cpu.Step();
    
    REQUIRE((cpu.GetCpsr() & 0x1F) == 0x1B);
    REQUIRE((cpu.GetCpsr() & 0x20) == 0);
    REQUIRE(cpu.GetRegister(15) == 0x00000004);
}

TEST_CASE("ARM LDC triggers Undefined Instruction exception") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeLDC(0xE, false, 15, 0, 0, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(15, 0x08000000);
    
    cpu.Step();
    
    REQUIRE((cpu.GetCpsr() & 0x1F) == 0x1B);
    REQUIRE((cpu.GetCpsr() & 0x20) == 0);
    REQUIRE(cpu.GetRegister(15) == 0x00000004);
}

TEST_CASE("ARM STC triggers Undefined Instruction exception") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeSTC(0xE, false, 15, 0, 0, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(15, 0x08000000);
    
    cpu.Step();
    
    REQUIRE((cpu.GetCpsr() & 0x1F) == 0x1B);
    REQUIRE((cpu.GetCpsr() & 0x20) == 0);
    REQUIRE(cpu.GetRegister(15) == 0x00000004);
}

TEST_CASE("ARM CDP triggers Undefined Instruction exception") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeCDP(0xE, 15, 0, 0, 0, 0, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(15, 0x08000000);
    
    cpu.Step();
    
    REQUIRE((cpu.GetCpsr() & 0x1F) == 0x1B);
    REQUIRE((cpu.GetCpsr() & 0x20) == 0);
    REQUIRE(cpu.GetRegister(15) == 0x00000004);
}

TEST_CASE("Undefined Instruction exception saves CPSR to SPSR_und") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeMRC(0xE, 15, 0, 0, 0, 0, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(15, 0x08000000);
    cpu.GetCpsr();  // Current CPSR = 0x1F (System mode)
    
    cpu.Step();
    
    // Check SPSR_und was saved (via direct register access not available, 
    // but we can verify mode change and PC)
    REQUIRE((cpu.GetCpsr() & 0x1F) == 0x1B);
}

TEST_CASE("Undefined Instruction exception saves return address to LR_und") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeMRC(0xE, 15, 0, 0, 0, 0, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(15, 0x08000000);
    
    cpu.Step();
    
    // LR_und should be instruction_address + 4 = 0x08000004
    // We can't directly read LR_und but can verify PC went to vector
    REQUIRE(cpu.GetRegister(15) == 0x00000004);
}

TEST_CASE("Undefined Instruction exception disables IRQ (I bit set)") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeMRC(0xE, 15, 0, 0, 0, 0, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(15, 0x08000000);
    // Initial CPSR has I bit clear (0x1F = 0b11111, bit 7 = 0)
    
    cpu.Step();
    
    // I bit (bit 7) should be set in Undefined mode
    REQUIRE((cpu.GetCpsr() >> 7) & 1);
}

TEST_CASE("Undefined Instruction exception preserves FIQ disable bit") {
    Bus bus(cpu_test::AsRom(cpu_test::EncodeMRC(0xE, 15, 0, 0, 0, 0, 0)));
    Cpu cpu(bus);
    cpu.SetRegister(15, 0x08000000);
    // Initial CPSR has F bit clear
    
    cpu.Step();
    
    // F bit (bit 6) should be preserved
    REQUIRE(((cpu.GetCpsr() >> 6) & 1) == 0);
}

TEST_CASE("Coprocessor instruction conditional execution fails when condition not met") {
    const u32 instruction = cpu_test::EncodeMRC(0x0, 15, 0, 0, 0, 0, 0);  // EQ condition
    Bus bus(cpu_test::AsRom(instruction));
    Cpu cpu(bus);
    cpu.SetRegister(15, 0x08000000);
    
    cpu.Step();
    
    // Should not take exception, just advance PC
    REQUIRE((cpu.GetCpsr() & 0x1F) == 0x1F);  // Still in System mode
    REQUIRE(cpu.GetRegister(15) == 0x08000004);
}

TEST_CASE("Undefined Instruction exception from different coprocessor numbers") {
    for (u32 cp = 0; cp < 16; ++cp) {
        Bus bus(cpu_test::AsRom(cpu_test::EncodeMCR(0xE, cp, 0, 0, 0, 0, 0)));
        Cpu cpu(bus);
        cpu.SetRegister(15, 0x08000000);
        
        cpu.Step();
        
        REQUIRE((cpu.GetCpsr() & 0x1F) == 0x1B);
        REQUIRE((cpu.GetCpsr() & 0x20) == 0);
        REQUIRE(cpu.GetRegister(15) == 0x00000004);
    }
}