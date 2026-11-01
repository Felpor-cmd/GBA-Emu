#include "cpu.hpp"
#include "cpu_detail.hpp"

#include <bit>

namespace {

constexpr u32 kNegativeFlag = 1u << 31;
constexpr u32 kZeroFlag = 1u << 30;
constexpr u32 kCarryFlag = 1u << 29;
constexpr u32 kOverflowFlag = 1u << 28;

struct ArithmeticResult { u32 value; u32 flags; };

ArithmeticResult Add(u32 a, u32 b, bool carry) {
    const u64 wide = static_cast<u64>(a) + b + static_cast<u32>(carry);
    const u32 result = static_cast<u32>(wide);
    return {result, (result & kNegativeFlag) | (result == 0 ? kZeroFlag : 0u) |
        (wide >> 32 ? kCarryFlag : 0u) |
        ((~(a ^ b) & (a ^ result) & kNegativeFlag) ? kOverflowFlag : 0u)};
}

struct ShiftResult { u32 value; bool carry; };

ShiftResult ShiftRegister(u32 value, u32 kind, u32 amount, bool old_carry) {
    amount &= 0xFFu;
    if (amount == 0) return {value, old_carry};
    if (kind == 0) {
        if (amount > 32) return {0, false};
        if (amount == 32) return {0, (value & 1u) != 0};
        return {value << amount, ((value >> (32 - amount)) & 1u) != 0};
    }
    if (kind == 1) {
        if (amount > 32) return {0, false};
        if (amount == 32) return {0, (value & kNegativeFlag) != 0};
        return {value >> amount, ((value >> (amount - 1)) & 1u) != 0};
    }
    if (kind == 2) {
        if (amount >= 32) return {(value & kNegativeFlag) ? 0xFFFFFFFFu : 0u,
                                  (value & kNegativeFlag) != 0};
        return {static_cast<u32>(static_cast<s32>(value) >> amount),
                ((value >> (amount - 1)) & 1u) != 0};
    }
    const u32 rotate = amount & 31u;
    if (rotate == 0) return {value, (value & kNegativeFlag) != 0};
    return {cpu_detail::RotateRight(value, rotate), ((value >> (rotate - 1)) & 1u) != 0};
}

}  // namespace

void Cpu::ExecuteThumbBranchExchange(u16 instruction) {
    u32 rm = (instruction >> 3) & 0xFu;
    u32 target = rm == 15 ? regs_[15] + 2 : regs_[rm];
    bool enter_thumb = (target & 1u) != 0;
    WritePsr(false, enter_thumb ? kThumbFlag : 0, kThumbFlag);
    regs_[15] = target & (enter_thumb ? ~1u : ~3u);
}

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

bool Cpu::ExecuteThumb(u16 insn, u32 address) {
    const u32 next_pc = address + 2;
    const u32 thumb_pc = address + 4;
    const u32 rd = insn & 7u;
    const u32 rs = (insn >> 3) & 7u;
    const bool carry = (cpsr_ & kCarryFlag) != 0;
    auto set_nz = [this](u32 value) {
        WritePsr(false, (value & kNegativeFlag) | (value == 0 ? kZeroFlag : 0u),
                 kNegativeFlag | kZeroFlag);
    };
    auto set_arithmetic = [this](ArithmeticResult result) {
        WritePsr(false, result.flags,
                 kNegativeFlag | kZeroFlag | kCarryFlag | kOverflowFlag);
    };
    auto load_word = [this](u32 effective) {
        return cpu_detail::RotateRight(ReadData32(effective & ~3u),
                                       (effective & 3u) * 8u);
    };
    auto load_half = [this](u32 effective) {
        const u32 half = ReadData16(effective & ~1u);
        return (effective & 1u) ? cpu_detail::RotateRight(half, 8) : half;
    };
    auto store_word = [this](u32 effective, u32 value) {
        WriteData32(effective & ~3u, value);
    };

    // The second BL half must immediately follow its first half.
    const bool second_bl_half = (insn & 0xF800u) == 0xF800u;
    const bool had_bl_prefix = thumb_bl_pending_ && address == thumb_bl_next_address_;
    thumb_bl_pending_ = false;

    if ((insn & 0xE000u) == 0) {
        if ((insn & 0x1800u) != 0x1800u) {
            ExecuteThumbMoveShiftedRegister(insn);
        } else {
            const u32 a = regs_[rs];
            const u32 b = (insn & 0x0400u) ? ((insn >> 6) & 7u) : regs_[(insn >> 6) & 7u];
            const bool subtract = (insn & 0x0200u) != 0;
            const auto result = subtract ? Add(a, ~b, true) : Add(a, b, false);
            regs_[rd] = result.value;
            set_arithmetic(result);
        }
        return false;
    }
    if ((insn & 0xE000u) == 0x2000u) {
        const u32 op = (insn >> 11) & 3u;
        const u32 dest = (insn >> 8) & 7u;
        const u32 imm = insn & 0xFFu;
        if (op == 0) { regs_[dest] = imm; set_nz(imm); }
        else if (op == 1) set_arithmetic(Add(regs_[dest], ~imm, true));
        else {
            const auto result = op == 2 ? Add(regs_[dest], imm, false)
                                        : Add(regs_[dest], ~imm, true);
            regs_[dest] = result.value;
            set_arithmetic(result);
        }
        return false;
    }
    if ((insn & 0xFC00u) == 0x4000u) {
        const u32 op = (insn >> 6) & 15u;
        const u32 a = regs_[rd], b = regs_[rs];
        u32 result = 0;
        bool write = true;
        bool arithmetic = false;
        bool shift = false;
        ShiftResult shifted{};
        switch (op) {
            case 0: result = a & b; break;
            case 1: result = a ^ b; break;
            case 2: case 3: case 4: case 7:
                shifted = ShiftRegister(a, op == 7 ? 3u : op - 2u, b, carry);
                result = shifted.value; shift = true; ChargeInternal(1);
                next_fetch_sequential_ = false;
                break;
            case 5: result = Add(a, b, carry).value; arithmetic = true; break;
            case 6: result = Add(a, ~b, carry).value; arithmetic = true; break;
            case 8: result = a & b; write = false; break;
            case 9: result = Add(0, ~b, true).value; arithmetic = true; break;
            case 10: result = Add(a, ~b, true).value; arithmetic = true; write = false; break;
            case 11: result = Add(a, b, false).value; arithmetic = true; write = false; break;
            case 12: result = a | b; break;
            case 13: {
                result = a * b;
                const s32 multiplier = static_cast<s32>(a);
                const u32 m = multiplier >= -128 && multiplier <= 127 ? 1u :
                    multiplier >= -32768 && multiplier <= 32767 ? 2u :
                    multiplier >= -8388608 && multiplier <= 8388607 ? 3u : 4u;
                ChargeInternal(m);
                next_fetch_sequential_ = false;
                break;
            }
            case 14: result = a & ~b; break;
            case 15: result = ~b; break;
        }
        if (write) regs_[rd] = result;
        if (arithmetic) {
            const auto flags = op == 9 ? Add(0, ~b, true) :
                               op == 11 ? Add(a, b, false) :
                               Add(a, ~b, op != 6 || carry);
            set_arithmetic(op == 5 ? Add(a, b, carry) : flags);
        } else if (shift) {
            WritePsr(false, (result & kNegativeFlag) | (result == 0 ? kZeroFlag : 0u) |
                     (shifted.carry ? kCarryFlag : 0u),
                     kNegativeFlag | kZeroFlag | kCarryFlag);
        } else set_nz(result);
        return false;
    }
    if ((insn & 0xFC00u) == 0x4400u) {
        const u32 op = (insn >> 8) & 3u;
        const u32 dest = rd | ((insn >> 4) & 8u);
        const u32 source = rs | ((insn >> 3) & 8u);
        if (op == 3) {
            if ((insn & 0x0087u) != 0) {
                if (insn & 0x0080u) { EnterUndefinedInstructionException(address); return true; }
                return false;
            }
            ExecuteThumbBranchExchange(insn);
            return true;
        }
        if (dest < 8 && source < 8) return false; // Unpredictable low/low encoding.
        const u32 source_value = source == 15 ? thumb_pc : regs_[source];
        const u32 dest_value = dest == 15 ? thumb_pc : regs_[dest];
        if (op == 1) { set_arithmetic(Add(dest_value, ~source_value, true)); return false; }
        const u32 result = op == 0 ? dest_value + source_value : source_value;
        regs_[dest] = dest == 15 ? result & ~1u : result;
        return dest == 15;
    }
    if ((insn & 0xF800u) == 0x4800u) {
        regs_[(insn >> 8) & 7u] = load_word((thumb_pc & ~3u) + ((insn & 0xFFu) << 2));
        ChargeInternal(1);
        return false;
    }
    if ((insn & 0xF000u) == 0x5000u) {
        const u32 op = (insn >> 9) & 7u;
        const u32 effective = regs_[rs] + regs_[(insn >> 6) & 7u];
        switch (op) {
            case 0: store_word(effective, regs_[rd]); break;
            case 1: WriteData16(effective & ~1u, static_cast<u16>(regs_[rd])); break;
            case 2: WriteData8(effective, static_cast<u8>(regs_[rd])); break;
            case 3: regs_[rd] = static_cast<u32>(static_cast<s32>(static_cast<s8>(ReadData8(effective)))); break;
            case 4: regs_[rd] = load_word(effective); break;
            case 5: regs_[rd] = load_half(effective); break;
            case 6: regs_[rd] = ReadData8(effective); break;
            case 7: regs_[rd] = (effective & 1u)
                ? static_cast<u32>(static_cast<s32>(static_cast<s8>(ReadData8(effective))))
                : static_cast<u32>(static_cast<s32>(static_cast<s16>(ReadData16(effective)))); break;
        }
        if (op >= 3) ChargeInternal(1);
        return false;
    }
    if ((insn & 0xE000u) == 0x6000u) {
        const bool byte = (insn & 0x1000u) != 0;
        const bool load = (insn & 0x0800u) != 0;
        const u32 effective = regs_[rs] + (((insn >> 6) & 31u) << (byte ? 0 : 2));
        if (load) regs_[rd] = byte ? ReadData8(effective) : load_word(effective);
        else if (byte) WriteData8(effective, static_cast<u8>(regs_[rd]));
        else store_word(effective, regs_[rd]);
        if (load) ChargeInternal(1);
        return false;
    }
    if ((insn & 0xF000u) == 0x8000u) {
        const u32 effective = regs_[rs] + (((insn >> 6) & 31u) << 1);
        if (insn & 0x0800u) { regs_[rd] = load_half(effective); ChargeInternal(1); }
        else WriteData16(effective & ~1u, static_cast<u16>(regs_[rd]));
        return false;
    }
    if ((insn & 0xF000u) == 0x9000u) {
        const u32 dest = (insn >> 8) & 7u;
        const u32 effective = regs_[13] + ((insn & 0xFFu) << 2);
        if (insn & 0x0800u) { regs_[dest] = load_word(effective); ChargeInternal(1); }
        else store_word(effective, regs_[dest]);
        return false;
    }
    if ((insn & 0xF000u) == 0xA000u) {
        regs_[(insn >> 8) & 7u] = ((insn & 0x0800u) ? regs_[13] : thumb_pc & ~3u)
                                  + ((insn & 0xFFu) << 2);
        return false;
    }
    if ((insn & 0xFF00u) == 0xB000u) {
        const u32 offset = (insn & 0x7Fu) << 2;
        regs_[13] += (insn & 0x80u) ? 0u - offset : offset;
        return false;
    }
    if ((insn & 0xF600u) == 0xB400u) {
        const bool pop = (insn & 0x0800u) != 0;
        const u32 mask = insn & 0xFFu;
        const u32 count = std::popcount(mask) + ((insn & 0x0100u) ? 1u : 0u);
        if (count == 0) {
            if (pop) {
                regs_[15] = ReadData32(regs_[13] & ~3u) & ~1u;
                regs_[13] += 0x40u;
                ChargeInternal(1);
                return true;
            }
            regs_[13] -= 0x40u;
            store_word(regs_[13], thumb_pc + 2);
            return false;
        }
        u32 location = pop ? regs_[13] : regs_[13] - count * 4u;
        const u32 new_sp = pop ? regs_[13] + count * 4u : location;
        for (u32 reg = 0; reg < 8; ++reg) {
            if (!(mask & (1u << reg))) continue;
            if (pop) regs_[reg] = ReadData32(location & ~3u);
            else store_word(location, regs_[reg]);
            location += 4;
        }
        if (insn & 0x0100u) {
            if (pop) regs_[15] = ReadData32(location & ~3u) & ~1u;
            else store_word(location, regs_[14]);
        }
        regs_[13] = new_sp;
        if (pop) ChargeInternal(1);
        return pop && (insn & 0x0100u);
    }
    if ((insn & 0xF000u) == 0xC000u) {
        const bool load = (insn & 0x0800u) != 0;
        const u32 base_reg = (insn >> 8) & 7u;
        const u32 base = regs_[base_reg];
        const u32 mask = insn & 0xFFu;
        if (mask == 0) {
            if (load) regs_[15] = ReadData32(base & ~3u) & ~1u;
            else store_word(base, thumb_pc + 2);
            regs_[base_reg] = base + 0x40u;
            if (load) ChargeInternal(1);
            return load;
        }
        const u32 count = std::popcount(mask);
        u32 location = base;
        const bool base_in_list = (mask & (1u << base_reg)) != 0;
        const bool base_first = (mask & ((1u << base_reg) - 1u)) == 0;
        for (u32 reg = 0; reg < 8; ++reg) {
            if (!(mask & (1u << reg))) continue;
            if (load) regs_[reg] = ReadData32(location & ~3u);
            else store_word(location, reg == base_reg && !base_first ? base + count * 4u : regs_[reg]);
            location += 4;
        }
        if (!load || !base_in_list) regs_[base_reg] = base + count * 4u;
        if (load) ChargeInternal(1);
        return false;
    }
    if ((insn & 0xF000u) == 0xD000u) {
        const u32 condition = (insn >> 8) & 15u;
        if (condition == 15u) { ExecuteSoftwareInterrupt(insn, address); return true; }
        if (condition == 14u) { EnterUndefinedInstructionException(address); return true; }
        if (!CheckCondition(condition, cpsr_)) return false;
        const s32 displacement = static_cast<s32>(static_cast<s8>(insn & 0xFFu)) * 2;
        regs_[15] = thumb_pc + static_cast<u32>(displacement);
        return true;
    }
    if ((insn & 0xF800u) == 0xE000u) {
        s32 displacement = insn & 0x7FFu;
        if (displacement & 0x400) displacement -= 0x800;
        regs_[15] = thumb_pc + static_cast<u32>(displacement * 2);
        return true;
    }
    if ((insn & 0xF800u) == 0xF000u) {
        s32 upper = insn & 0x7FFu;
        if (upper & 0x400) upper -= 0x800;
        regs_[14] = thumb_pc + static_cast<u32>(upper * 4096);
        thumb_bl_pending_ = true;
        thumb_bl_next_address_ = next_pc;
        return false;
    }
    if (second_bl_half) {
        if (!had_bl_prefix) return false; // Orphan second half is unpredictable.
        const u32 target = regs_[14] + ((insn & 0x7FFu) << 1);
        regs_[14] = next_pc | 1u;
        regs_[15] = target & ~1u;
        return true;
    }
    EnterUndefinedInstructionException(address);
    return true;
}
