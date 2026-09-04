#include "cpu.hpp"

#include <cstdio>

Cpu::Cpu(Bus& bus) : bus_(bus) {
    Reset();
}

bool Cpu::CheckCondition(u32 cond, u32 cpsr) {
    bool n = (cpsr >> 31) & 1;
    bool z = (cpsr >> 30) & 1;
    bool c = (cpsr >> 29) & 1;
    bool v = (cpsr >> 28) & 1;

    switch (cond) {
        case 0x0: return z;              // EQ
        case 0x1: return !z;             // NE
        case 0x2: return c;              // CS/HS
        case 0x3: return !c;             // CC/LO
        case 0x4: return n;              // MI
        case 0x5: return !n;             // PL
        case 0x6: return v;              // VS
        case 0x7: return !v;             // VC
        case 0x8: return c && !z;        // HI
        case 0x9: return !c || z;        // LS
        case 0xA: return n == v;         // GE
        case 0xB: return n != v;         // LT
        case 0xC: return !z && (n == v); // GT
        case 0xD: return z || (n != v);  // LE
        case 0xE: return true;           // AL
        default:  return false;          // NV
    }
}

void Cpu::Reset() {
    regs_.fill(0);
    shared_r8_r12_.fill(0);
    fiq_r8_r14_.fill(0);
    user_system_r13_r14_.fill(0);
    irq_r13_r14_.fill(0);
    svc_r13_r14_.fill(0);
    abt_r13_r14_.fill(0);
    und_r13_r14_.fill(0);
    spsr_fiq_ = 0;
    spsr_irq_ = 0;
    spsr_svc_ = 0;
    spsr_abt_ = 0;
    spsr_und_ = 0;
    regs_[13] = 0x03007F00;  // SP: matches the post-BIOS stack pointer (GBATEK: BIOS).
    user_system_r13_r14_[0] = regs_[13];
    regs_[15] = 0x08000000;  // instruction_address: cartridge ROM entry point.
    cpsr_ = 0x1F;             // System mode, ARM state, IRQ/FIQ unmasked.
}

void Cpu::Step() {
    bool thumb_mode = (cpsr_ & kThumbFlag) != 0;
    u32 instruction_address = regs_[15];

    if (thumb_mode) {
        u16 instruction = bus_.Read16(instruction_address);
        regs_[15] = instruction_address + 2;

        std::printf("Enter THUMB\n");

        // Thumb decoding will be implemented later.
        (void)instruction;
        return;
    }

    u32 instruction = bus_.Read32(instruction_address);
    regs_[15] = instruction_address + 4;
    std::printf("Enter ARM\n");

    u32 condition = instruction >> 28;
    if (!CheckCondition(condition, cpsr_)) {
        return;  // Condition not met; instruction is a no-op.
    }

    bool is_software_interrupt =
        (instruction & 0x0F000000u) == 0x0F000000u;

    // ARM branch encoding
    // bits 27-25 must be 101
    bool is_branch = ((instruction & 0x0E000000u) == 0x0A000000u);

    // BX is encoded in the ARM data-processing space.
    bool is_branch_exchange = ((instruction & 0x0FFFFFF0u) == 0x012FFF10u);

    // MUL and MLA are also encoded in the ARM data-processing space.
    bool is_multiply = ((instruction & 0x0FC000F0u) == 0x00000090u);

    bool is_mrs = ((instruction & 0x0FBF0FFFu) == 0x010F0000u);
    bool is_msr_register =
        ((instruction & 0x0FB0FFF0u) == 0x0120F000u);
    bool is_msr_immediate =
        ((instruction & 0x0FB0F000u) == 0x0320F000u);

    if (is_software_interrupt) {
        ExecuteSoftwareInterrupt(instruction, instruction_address);
        return;
    }

    if (is_branch_exchange) {
        ExecuteBranchExchange(instruction);
        return;
    }

    if (is_multiply) {
        ExecuteMultiply(instruction);
        return;
    }

    if (is_mrs) {
        ExecuteMrs(instruction);
        return;
    }

    if (is_msr_register) {
        ExecuteMsr(instruction, false);
        return;
    }

    if (is_msr_immediate) {
        ExecuteMsr(instruction, true);
        return;
    }

    if (is_branch) {
        ExecuteBranch(instruction, instruction_address);
        return;
    }

    bool is_halfword_transfer =
        (instruction & 0x0E000000u) == 0 &&
        (instruction & 0x00000090u) == 0x00000090u &&
        (instruction & 0x00000060u) != 0;

    if (is_halfword_transfer) {
        ExecuteHalfwordDataTransfer(instruction, instruction_address);
        return;
    }

    bool is_block_data_transfer = ((instruction >> 25) & 0x7) == 0b100;

    if (is_block_data_transfer) {
        ExecuteBlockDataTransfer(instruction, instruction_address);
        return;
    }

    u32 category = (instruction >> 26) & 0b11;

    if (category == 0b00) {
        ExecuteDataProcessing(instruction);
    } else if (category == 0b01) {
        ExecuteSingleDataTransfer(instruction, instruction_address);
    }
}
