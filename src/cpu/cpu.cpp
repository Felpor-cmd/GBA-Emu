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
    regs_[13] = 0x03007F00;  // SP: matches the post-BIOS stack pointer (GBATEK: BIOS).
    regs_[15] = 0x08000000;  // PC: cartridge ROM entry point.
    cpsr_ = 0x1F;             // System mode, ARM state, IRQ/FIQ unmasked.
}

void Cpu::Step() {
    u32 instruction = bus_.Read32(regs_[15]);  
    regs_[15] += 4;                            
    // decoding/executing `instruction` comes in a later milestone — nothing else here yet

    bool thumb_mode = (cpsr_ & kThumbFlag) != 0;

    if (thumb_mode){
        u16 thumb_instruction = bus_.Read16(regs_[15]);
        regs_[15] += 2; 
        std::printf("Enter THUMB\n");

    } else {
        u32 instruction = bus_.Read32(regs_[15]);
        regs_[15] += 4;
        std::printf("Enter ARM\n");
    }

    u32 cond = instruction >> 28;
    bool should_execute = CheckCondition(cond, cpsr_);
}