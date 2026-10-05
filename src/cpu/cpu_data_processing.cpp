#include "cpu.hpp"

#include "cpu_detail.hpp"

namespace {

struct AddResult { u32 value; bool carry; bool overflow; };

AddResult AddWithCarry(u32 a, u32 b, bool carry_in) {
    u64 result64 = static_cast<u64>(a) + static_cast<u64>(b) +
                   (carry_in ? 1u : 0u);
    u32 result = static_cast<u32>(result64);
    bool carry = result64 > 0xFFFFFFFFull;
    bool overflow = ((~(a ^ b) & (a ^ result)) >> 31) & 1;
    return {result, carry, overflow};
}

struct ShiftResult { u32 value; bool carry; };

ShiftResult ShiftWithCarry(u32 value, u32 shift_type, u32 amount, bool carry_in) {
    bool carry = carry_in;

    switch (shift_type) {
        case 0: { // LSL
            if (amount == 0) {
                // LSL #0: result = value, carry = old C flag
                return {value, carry_in};
            }
            if (amount >= 32) {
                carry = (amount == 32) ? ((value & 1u) != 0) : false;
                return {0, carry};
            }
            carry = ((value >> (32 - amount)) & 1u) != 0;
            return {value << amount, carry};
        }
        case 1: { // LSR
            if (amount == 0) {
                // LSR #0 means 32
                carry = ((value >> 31) & 1u) != 0;
                return {0, carry};
            }
            if (amount >= 32) {
                carry = (amount == 32) ? ((value >> 31) & 1u) != 0 : false;
                return {0, carry};
            }
            carry = ((value >> (amount - 1)) & 1u) != 0;
            return {value >> amount, carry};
        }
        case 2: { // ASR
            if (amount == 0) {
                // ASR #0 means 32
                carry = ((value >> 31) & 1u) != 0;
                if (carry) {
                    return {0xFFFFFFFFu, true};
                }
                return {0, false};
            }
            if (amount >= 32) {
                carry = ((value >> 31) & 1u) != 0;
                if (carry) {
                    return {0xFFFFFFFFu, true};
                }
                return {0, false};
            }
            carry = ((value >> (amount - 1)) & 1u) != 0;
            u32 result = static_cast<u32>(static_cast<s32>(value) >> amount);
            return {result, carry};
        }
        case 3: { // ROR / RRX
            if (amount == 0) {
                // RRX: rotate right by 1 through carry
                carry = (value & 1u) != 0;
                return {(carry_in ? 0x80000000u : 0u) | (value >> 1), carry};
            }
            amount &= 31;
            carry = ((value >> (amount - 1)) & 1u) != 0;
            return {cpu_detail::RotateRight(value, amount), carry};
        }
        default:
            return {value, carry};
    }
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

        op2 = cpu_detail::RotateRight(imm8, rotate_amount);

        // An immediate with no rotation does not produce a new carry value;
        // the old CPSR C flag is carried through instead.
        if (rotate_amount != 0) {
            shifter_carry_out = (op2 >> 31) & 1;
        }
    } else {
        u32 rm = instruction & 0xF;
        u32 shift_type = (instruction >> 5) & 0x3;
        bool register_specified_shift = (instruction >> 4) & 1;

        if (register_specified_shift) {
            // Shift amount from bottom 8 bits of Rs register
            u32 rs = (instruction >> 8) & 0xF;
            u32 shift_amount = regs_[rs] & 0xFF;
            auto shifted = ShiftWithCarry(regs_[rm], shift_type, shift_amount, c_in);
            op2 = shifted.value;
            shifter_carry_out = shifted.carry;
        } else {
            // Shift amount from immediate (bits 11-7)
            u32 shift_amount = (instruction >> 7) & 0x1F;
            auto shifted = ShiftWithCarry(regs_[rm], shift_type, shift_amount, c_in);
            op2 = shifted.value;
            shifter_carry_out = shifted.carry;
        }
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
        WritePsr(false, (n << 31) | (z << 30) | (c << 29) | (v << 28),
                 0xF0000000u);
    }
}

void Cpu::ExecuteMultiply(u32 instruction) {
    bool accumulate = (instruction >> 21) & 1;
    bool set_flags = (instruction >> 20) & 1;
    u32 rd = (instruction >> 16) & 0xF;
    u32 rn = (instruction >> 12) & 0xF;
    u32 rs = (instruction >> 8) & 0xF;
    u32 rm = instruction & 0xF;

    bool invalid_registers = rd == 15 || rs == 15 || rm == 15 || rd == rm;
    bool invalid_accumulator = accumulate ? rn == 15 : rn != 0;
    if (invalid_registers || invalid_accumulator) {
        return;
    }

    u32 rm_value = regs_[rm];
    u32 rs_value = regs_[rs];
    u32 rn_value = accumulate ? regs_[rn] : 0;

    u64 result64 = static_cast<u64>(rm_value) * static_cast<u64>(rs_value);
    if (accumulate) {
        result64 += rn_value;
    }
    u32 result = static_cast<u32>(result64);

    regs_[rd] = result;

    if (set_flags) {
        constexpr u32 kNegativeFlag = 1u << 31;
        constexpr u32 kZeroFlag = 1u << 30;
        WritePsr(false, (result & kNegativeFlag) |
                          (result == 0 ? kZeroFlag : 0),
                 kNegativeFlag | kZeroFlag);
    }
}

void Cpu::ExecuteLongMultiply(u32 instruction) {
    bool accumulate = (instruction >> 21) & 1;
    bool set_flags = (instruction >> 20) & 1;
    bool is_signed = (instruction >> 23) & 1;
    u32 rd_lo = (instruction >> 16) & 0xF;  // Destination low
    u32 rd_hi = (instruction >> 12) & 0xF;  // Destination high
    u32 rs = (instruction >> 8) & 0xF;      // Source register
    u32 rm = instruction & 0xF;             // Multiplier register

    // ARM7TDMI operand restrictions for long multiply:
    // - RdLo, RdHi, Rs, Rm cannot be r15 (PC)
    // - RdLo cannot equal Rm
    // - RdHi cannot equal Rm
    // - For accumulate: RdLo and RdHi are both destinations
    bool invalid_regs = rd_lo == 15 || rd_hi == 15 || rs == 15 || rm == 15 ||
                        rd_lo == rm || rd_hi == rm;
    // For accumulate, RdHi cannot be 15, and both RdLo and RdHi are written
    if (invalid_regs) {
        return;
    }

    u32 rm_value = regs_[rm];
    u32 rs_value = regs_[rs];

    u64 result64 = 0;
    if (is_signed) {
        // SMULL/SMLAL: signed multiply
        s32 rm_signed = static_cast<s32>(rm_value);
        s32 rs_signed = static_cast<s32>(rs_value);
        s64 product = static_cast<s64>(rm_signed) * static_cast<s64>(rs_signed);
        result64 = static_cast<u64>(product);
    } else {
        // UMULL/UMLAL: unsigned multiply
        result64 = static_cast<u64>(rm_value) * static_cast<u64>(rs_value);
    }

    if (accumulate) {
        // Read existing 64-bit accumulator value (RdHi:RdLo)
        u64 acc = (static_cast<u64>(regs_[rd_hi]) << 32) | regs_[rd_lo];
        result64 += acc;
    }

    // Write low 32 bits to RdLo, high 32 bits to RdHi
    regs_[rd_lo] = static_cast<u32>(result64);
    regs_[rd_hi] = static_cast<u32>(result64 >> 32);

    if (set_flags) {
        // N flag: bit 63 of the 64-bit result (MSB of high word)
        // Z flag: 1 if entire 64-bit result is zero
        // C and V are unpredictable for long multiply on ARM7TDMI - leave unchanged
        u32 n = (result64 >> 63) & 1;
        u32 z = (result64 == 0) ? 1u : 0u;
        WritePsr(false, (n << 31) | (z << 30), 0xC0000000u);
    }
}
