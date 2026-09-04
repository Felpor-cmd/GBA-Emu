#include "cpu.hpp"

namespace {

constexpr u32 kNegativeFlag = 1u << 31;
constexpr u32 kZeroFlag = 1u << 30;
constexpr u32 kCarryFlag = 1u << 29;

}  // namespace

void Cpu::ExecuteThumbMoveShiftedRegister(u16 instruction) {
    u32 opcode = (instruction >> 11) & 0x3;
    u32 shift_amount = (instruction >> 6) & 0x1F;
    u32 rs = (instruction >> 3) & 0x7;
    u32 rd = instruction & 0x7;

    u32 source = regs_[rs];
    u32 result = 0;
    bool carry = (cpsr_ & kCarryFlag) != 0;

    switch (opcode) {
        case 0:  // LSL
            if (shift_amount == 0) {
                result = source;
            } else {
                result = source << shift_amount;
                carry = ((source >> (32 - shift_amount)) & 1) != 0;
            }
            break;
        case 1:  // LSR
            if (shift_amount == 0) {
                result = 0;
                carry = ((source >> 31) & 1) != 0;
            } else {
                result = source >> shift_amount;
                carry = ((source >> (shift_amount - 1)) & 1) != 0;
            }
            break;
        case 2:  // ASR
            if (shift_amount == 0) {
                carry = ((source >> 31) & 1) != 0;
                result = carry ? 0xFFFFFFFFu : 0;
            } else {
                carry = ((source >> (shift_amount - 1)) & 1) != 0;
                result = source >> shift_amount;
                if ((source & kNegativeFlag) != 0) {
                    result |= 0xFFFFFFFFu << (32 - shift_amount);
                }
            }
            break;
        default:
            return;
    }

    regs_[rd] = result;

    u32 flags = result & kNegativeFlag;
    if (result == 0) {
        flags |= kZeroFlag;
    }
    if (carry) {
        flags |= kCarryFlag;
    }
    WritePsr(false, flags, kNegativeFlag | kZeroFlag | kCarryFlag);
}
