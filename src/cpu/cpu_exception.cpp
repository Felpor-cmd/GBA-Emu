#include "cpu.hpp"

#include <cstdio>
#include <stdexcept>

constexpr u32 kUndefinedMode = 0x1B;
constexpr u32 kUndefinedVector = 0x00000004;
constexpr u32 kSupervisorMode = 0x13;
constexpr u32 kSupervisorVector = 0x00000008;
constexpr u32 kIRQDisableBit = 0x80;  // bit 7

void Cpu::EnterUndefinedInstructionException(u32 instruction_address) {
    u32 cpsr = cpsr_;

    // Save CPSR to SPSR_und
    spsr_und_ = cpsr;

    // Save return address (instruction_address + 4) to LR_und
    und_r13_r14_[1] = instruction_address + ((cpsr & kThumbFlag) ? 2u : 4u);

    // Enter Undefined mode, switch to ARM state, and mask IRQs. WritePsr
    // switches the active register bank while preserving the outgoing bank.
    u32 exception_cpsr = (cpsr_ & ~0x3Fu) | kUndefinedMode;
    exception_cpsr &= ~kThumbFlag;
    exception_cpsr |= kIRQDisableBit;
    WritePsr(false, exception_cpsr, 0xFFFFFFFFu);

    // Branch to vector 0x00000004
    regs_[15] = kUndefinedVector;
}

void Cpu::EnterSoftwareInterruptException(u32 instruction_address) {
    spsr_svc_ = cpsr_;
    svc_r13_r14_[1] = instruction_address + ((cpsr_ & kThumbFlag) ? 2u : 4u);
    u32 exception_cpsr = (cpsr_ & ~(0x1Fu | kThumbFlag)) |
                         kSupervisorMode | kIRQDisableBit;
    WritePsr(false, exception_cpsr, 0xFFFFFFFFu);
    regs_[15] = kSupervisorVector;
}

bool Cpu::ServiceInterruptBoundary() {
    if (hle_irq_active_ && regs_[15] == 0x00000138u) {
        for (int reg : {0, 1, 2, 3, 12, 14}) {
            regs_[reg] = ReadData32(regs_[13]);
            regs_[13] += 4;
        }
        const u32 resume = regs_[14] - 4;
        const u32 saved = spsr_irq_;
        WritePsr(false, saved, 0xFFFFFFFFu);
        regs_[15] = resume;
        hle_irq_active_ = false;
        if (bios_wait_mask_) {
            const u16 flags = bus_.Read16(0x03007FF8u);
            if (flags & bios_wait_mask_) {
                bus_.Write16(0x03007FF8u, flags & ~bios_wait_mask_);
                bios_wait_mask_ = 0;
            } else halted_ = true;
        }
        ChargeRefill(resume, (saved & kThumbFlag) ? 2 : 4);
        return true;
    }
    if (bus_.InterruptPending() && !bios_wait_mask_) halted_ = false;
    if (!bus_.IrqEnabled() || (cpsr_ & kIRQDisableBit)) return false;
    halted_ = false;
    const u32 resume = regs_[15];
    spsr_irq_ = cpsr_;
    irq_r13_r14_[1] = resume + 4;
    WritePsr(false, (cpsr_ & ~0x3Fu) | 0x12u | kIRQDisableBit, 0xFFFFFFFFu);
    regs_[15] = 0x18;
    if (emulate_bios_ && !bus_.HasBios()) {
        regs_[13] -= 24;
        u32 address = regs_[13];
        for (int reg : {0, 1, 2, 3, 12, 14}) {
            WriteData32(address, regs_[reg]);
            address += 4;
        }
        regs_[0] = 0x04000000u;
        regs_[14] = 0x138;
        regs_[15] = bus_.Read32(0x03007FFCu) & ~3u;
        if (!regs_[15]) throw std::runtime_error("IRQ requested without a game handler");
        hle_irq_active_ = true;
    }
    ChargeRefill(regs_[15], 4);
    return true;
}
