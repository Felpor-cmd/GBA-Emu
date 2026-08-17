#include "cpu.hpp"
#include <cstdio>

Cpu::Cpu(Bus& bus) : bus_(bus) {
    Reset();
}

void Cpu::Reset() {
    regs_.fill(0);
    regs_[13] = 0x03007F00;  // SP: matches the post-BIOS stack pointer (GBATEK: BIOS).
    regs_[15] = 0x08000000;  // PC: cartridge ROM entry point.
    cpsr_ = 0x1F;             // System mode, ARM state, IRQ/FIQ unmasked.
}

void Cpu::Step() {
    u32 instruction = bus_.Read32(regs_[15]);  
    regs_[15] += 4;                            
    // decoding/executing `instruction` comes in a later milestone — nothing else here yet

    std::printf("PC=0x%08X  instruction=0x%08X\n", regs_[15], instruction);
}