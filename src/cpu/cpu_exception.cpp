#include "cpu.hpp"

#include <cstdio>

constexpr u32 kUndefinedMode = 0x1B;
constexpr u32 kUndefinedVector = 0x00000004;
constexpr u32 kIRQDisableBit = 0x80;  // bit 7

void Cpu::EnterUndefinedInstructionException(u32 instruction_address) {
    u32 cpsr = cpsr_;

    // Save CPSR to SPSR_und
    spsr_und_ = cpsr;

    // Save return address (instruction_address + 4) to LR_und
    und_r13_r14_[1] = instruction_address + 4;

    // Enter Undefined mode, switch to ARM state, and mask IRQs. WritePsr
    // switches the active register bank while preserving the outgoing bank.
    u32 exception_cpsr = (cpsr_ & ~0x3Fu) | kUndefinedMode;
    exception_cpsr &= ~kThumbFlag;
    exception_cpsr |= kIRQDisableBit;
    WritePsr(false, exception_cpsr, 0xFFFFFFFFu);

    // Branch to vector 0x00000004
    regs_[15] = kUndefinedVector;
}
