#include "cpu.hpp"

void Cpu::ExecuteBranch(u32 instruction, u32 instruction_address) {
    // Bit 24 distinguishes B from BL.
    bool link = (instruction >> 24) & 1;

    // Extract and sign-extend the 24-bit branch offset.
    s32 offset = static_cast<s32>(instruction & 0x00FFFFFFu);

    if (offset & 0x00800000) {
        offset |= static_cast<s32>(0xFF000000u);
    }

    // ARM branch offsets are word-aligned.
    offset *= 4;

    // ARM's architectural PC is instruction address + 8.
    u32 target = instruction_address + 8u
               + static_cast<u32>(offset);

    if (link) {
        // BL stores the return address in LR/r14.
        regs_[14] = instruction_address + 4;
    }

    // B and BL both update the program counter.
    regs_[15] = target;
}

void Cpu::ExecuteBranchExchange(u32 instruction) {
    u32 rm = instruction & 0xF;
    u32 target = regs_[rm];
    bool enter_thumb = (target & 1) != 0;

    WritePsr(false, enter_thumb ? kThumbFlag : 0, kThumbFlag);

    regs_[15] = target & ~1u;
}
