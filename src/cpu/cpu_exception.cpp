#include "cpu.hpp"

#include <cstdio>

constexpr u32 kUndefinedMode = 0x1B;
constexpr u32 kUndefinedVector = 0x00000004;
constexpr u32 kModeMask = 0x1F;
constexpr u32 kIRQDisableBit = 0x80;  // bit 7
constexpr u32 kFIQDisableBit = 0x40;  // bit 6

void Cpu::EnterUndefinedInstructionException(u32 instruction_address) {
    u32 cpsr = cpsr_;

    // Save CPSR to SPSR_und
    spsr_und_ = cpsr;

    // Save return address (instruction_address + 4) to LR_und
    und_r13_r14_[1] = instruction_address + 4;

    // Enter Undefined mode
    cpsr_ = (cpsr_ & ~kModeMask) | kUndefinedMode;

    // Switch to ARM state (clear T bit)
    cpsr_ &= ~0x20;

    // Set IRQ disable bit (I bit)
    cpsr_ |= kIRQDisableBit;

    // Preserve FIQ disable bit (F bit) - already in cpsr_

    // Branch to vector 0x00000004
    regs_[15] = kUndefinedVector;
}