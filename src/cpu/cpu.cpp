#include "cpu.hpp"
#include <cstdio>

Cpu::Cpu(Bus& bus) : bus_(bus) {
    Reset();
}

namespace {

    struct AddResult { u32 value; bool carry; bool overflow; };

    AddResult AddWithCarry(u32 a, u32 b, bool carry_in) {
        u64 result64 = static_cast<u64>(a) + static_cast<u64>(b) + (carry_in ? 1u : 0u);
        u32 result = static_cast<u32>(result64);
        bool carry = result64 > 0xFFFFFFFFull;
        bool overflow = ((~(a ^ b) & (a ^ result)) >> 31) & 1;
        return {result, carry, overflow};
    }

}  // namespace

void Cpu::ExecuteDataProcessing(u32 instruction) {
    u32 opcode = (instruction >> 21) & 0xF;
    bool set_flags = (instruction >> 20) & 1;
    u32 rn = (instruction >> 16) & 0xF;
    u32 rd = (instruction >> 12) & 0xF;
    u32 rm = instruction & 0xF;  // this milestone: Operand2 == Rm, no shift yet

    u32 op1 = regs_[rn];
    u32 op2 = regs_[rm];
    bool c_in = (cpsr_ >> 29) & 1;

    u32 result = 0;
    bool write_result = true;
    bool carry_out = c_in;                  // unchanged unless arithmetic says otherwise
    bool overflow_out = (cpsr_ >> 28) & 1;   // unchanged unless arithmetic says otherwise

    switch (opcode) {
        case 0x0: result = op1 & op2; break;                                              // AND
        case 0x1: result = op1 ^ op2; break;                                              // EOR
        case 0x2: { auto r = AddWithCarry(op1, ~op2, true);  result = r.value; carry_out = r.carry; overflow_out = r.overflow; break; } // SUB
        case 0x3: { auto r = AddWithCarry(op2, ~op1, true);  result = r.value; carry_out = r.carry; overflow_out = r.overflow; break; } // RSB
        case 0x4: { auto r = AddWithCarry(op1, op2, false);  result = r.value; carry_out = r.carry; overflow_out = r.overflow; break; } // ADD
        case 0x5: { auto r = AddWithCarry(op1, op2, c_in);   result = r.value; carry_out = r.carry; overflow_out = r.overflow; break; } // ADC
        case 0x6: { auto r = AddWithCarry(op1, ~op2, c_in);  result = r.value; carry_out = r.carry; overflow_out = r.overflow; break; } // SBC
        case 0x7: { auto r = AddWithCarry(op2, ~op1, c_in);  result = r.value; carry_out = r.carry; overflow_out = r.overflow; break; } // RSC
        case 0x8: result = op1 & op2; write_result = false; break;                        // TST
        case 0x9: result = op1 ^ op2; write_result = false; break;                        // TEQ
        case 0xA: { auto r = AddWithCarry(op1, ~op2, true);  result = r.value; carry_out = r.carry; overflow_out = r.overflow; write_result = false; break; } // CMP
        case 0xB: { auto r = AddWithCarry(op1, op2, false);  result = r.value; carry_out = r.carry; overflow_out = r.overflow; write_result = false; break; } // CMN
        case 0xC: result = op1 | op2; break;                                              // ORR
        case 0xD: result = op2; break;                                                    // MOV
        case 0xE: result = op1 & ~op2; break;                                             // BIC
        case 0xF: result = ~op2; break;                                                   // MVN
    }

    if (write_result) {
        regs_[rd] = result;
    }

    if (set_flags) {
        u32 n = (result >> 31) & 1;
        u32 z = (result == 0) ? 1u : 0u;
        u32 c = carry_out ? 1u : 0u;
        u32 v = overflow_out ? 1u : 0u;
        cpsr_ = (cpsr_ & 0x0FFFFFFF) | (n << 31) | (z << 30) | (c << 29) | (v << 28);
    }
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
    bool thumb_mode = (cpsr_ & kThumbFlag) != 0;
    u32 pc = regs_[15];

    if (thumb_mode) {
        u16 instruction = bus_.Read16(pc);
        regs_[15] = pc + 2;
        std::printf("Enter THUMB\n");
        (void)instruction;  // decode/execute is a future milestone
    } else {
        u32 instruction = bus_.Read32(pc);
        regs_[15] = pc + 4;
        std::printf("Enter ARM\n");

        u32 cond = instruction >> 28;
        if (CheckCondition(cond, cpsr_)) {
            u32 category = (instruction >> 26) & 0b11;
            if (category == 0b00) {
                ExecuteDataProcessing(instruction);
            }
        }
    }
}