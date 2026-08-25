#include "cpu.hpp"
#include <cstdio>

Cpu::Cpu(Bus& bus) : bus_(bus) {
    Reset();
}

namespace {

    struct AddResult { u32 value; bool carry; bool overflow; };

    u32 RotateRight(u32 value, unsigned amount) {
        amount &= 31;
        if (amount == 0) {
            return value;
        }
        return (value >> amount) | (value << (32 - amount));
    }

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
    bool immediate_operand = (instruction >> 25) & 1;

    u32 op1 = regs_[rn];
    bool c_in = (cpsr_ >> 29) & 1;

    u32 op2 = 0;
    bool shifter_carry_out = c_in;

    if (immediate_operand) {
        u32 imm8 = instruction & 0xFF;
        unsigned rotate_amount = ((instruction >> 8) & 0xF) * 2;

        op2 = RotateRight(imm8, rotate_amount);

        // An immediate with no rotation does not produce a new carry value;
        // the old CPSR C flag is carried through instead.
        if (rotate_amount != 0) {
            shifter_carry_out = (op2 >> 31) & 1;
        }
    } else {
        u32 rm = instruction & 0xF;
        op2 = regs_[rm];
        // Register shifts are not implemented yet. An unshifted register
        // operand leaves the shifter carry equal to the old CPSR C flag.
    }

    u32 result = 0;
    bool write_result = true;
    bool carry_out = shifter_carry_out;     // overridden by arithmetic operations
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

    if (enter_thumb) 
        cpsr_ |= kThumbFlag;
    else 
        cpsr_ &= ~kThumbFlag;
    
    regs_[15] = target & ~1u;
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
    if(!CheckCondition(condition, cpsr_)) {
        return;  // Condition not met; instruction is a no-op.
    }

    // ARM branch encoding
    // bits 27-25 must be 101
    bool is_branch = ((instruction & 0x0E000000u) == 0x0A000000u);

    // BX is encoded in the ARM data-processing space.
    bool is_branch_exchange = ((instruction & 0x0FFFFFF0u) == 0x012FFF10u);

    if (is_branch_exchange) {
        ExecuteBranchExchange(instruction);
        return;
    }

    if(is_branch) {
        ExecuteBranch(instruction, instruction_address);
        return;
    }

    // Data-processing instruction belong to category 00.

    u32 category = (instruction >> 26) & 0xb11;

    if(category == 0b00) {
        ExecuteDataProcessing(instruction);
    }
}
