#include "cpu.hpp"

#include <cstdio>

#include "cpu_detail.hpp"

namespace {

u32 ShiftRegisterOffset(u32 value, unsigned shift_type, unsigned amount,
                        bool carry_in) {
    switch (shift_type) {
        case 0: // LSL
            return amount >= 32 ? 0 : value << amount;
        case 1: // LSR
            if (amount == 0) {
                amount = 32;
            }
            return amount >= 32 ? 0 : value >> amount;
        case 2: // ASR
            if (amount == 0) {
                amount = 32;
            }
            if (amount >= 32) {
                return (value & 0x80000000u) ? 0xFFFFFFFFu : 0;
            }
            return static_cast<u32>(static_cast<s32>(value) >> amount);
        case 3: // ROR / RRX
            if (amount == 0) {
                return (carry_in ? 0x80000000u : 0) | (value >> 1);
            }
            return cpu_detail::RotateRight(value, amount);
        default:
            return value;
    }
}

}  // namespace

u32 Cpu::GetUserRegister(u32 index) const {
    u32 mode = cpsr_ & 0x1Fu;
    if (index >= 8 && index <= 12 && mode == 0x11) {
        return shared_r8_r12_[index - 8];
    }
    if (index >= 13 && index <= 14 && mode != 0x10 && mode != 0x1F) {
        return user_system_r13_r14_[index - 13];
    }
    return regs_[index];
}

void Cpu::SetUserRegister(u32 index, u32 value) {
    u32 mode = cpsr_ & 0x1Fu;
    if (index >= 8 && index <= 12 && mode == 0x11) {
        shared_r8_r12_[index - 8] = value;
    } else if (index >= 13 && index <= 14 && mode != 0x10 && mode != 0x1F) {
        user_system_r13_r14_[index - 13] = value;
    } else {
        regs_[index] = value;
    }
}

void Cpu::ExecuteSingleDataTransfer(u32 instruction, u32 instruction_address) {
    bool register_offset = (instruction >> 25) & 1;
    bool pre_index = (instruction >> 24) & 1;
    bool add_offset = (instruction >> 23) & 1;
    bool byte_transfer = (instruction >> 22) & 1;
    bool writeback = (instruction >> 21) & 1;
    bool load = (instruction >> 20) & 1;
    u32 rn = (instruction >> 16) & 0xF;
    u32 rd = (instruction >> 12) & 0xF;

    u32 base = GetRegisterWithPC(rn, instruction_address);
    u32 offset = 0;

    if (register_offset) {
        u32 rm = instruction & 0xF;
        u32 offset_value = GetRegisterWithPC(rm, instruction_address);
        unsigned shift_amount = (instruction >> 7) & 0x1F;
        unsigned shift_type = (instruction >> 5) & 0x3;
        offset = ShiftRegisterOffset(offset_value, shift_type, shift_amount,
                                      (cpsr_ >> 29) & 1);
    } else {
        offset = instruction & 0xFFF;
    }

    u32 adjusted_offset = add_offset ? offset : 0 - offset;
    u32 effective_address = pre_index ? base + adjusted_offset : base;

    if (load) {
        u32 loaded_value;
        if (byte_transfer) {
            loaded_value = bus_.Read8(effective_address);
        } else {
            u32 aligned_addr = effective_address & ~3u;
            u32 word = bus_.Read32(aligned_addr);
            unsigned rotate = (effective_address & 3) * 8;
            loaded_value = (word >> rotate) | (word << (32 - rotate));
        }
        if (rd == 15) {
            regs_[15] = loaded_value & ~3u;
        } else {
            regs_[rd] = loaded_value;
        }
    } else if (byte_transfer) {
        bus_.Write8(effective_address, static_cast<u8>(regs_[rd]));
    } else {
        u32 aligned_addr = effective_address & ~3u;
        bus_.Write32(aligned_addr, regs_[rd]);
    }

    // Handle writeback: if Rn==Rd during load with writeback, loaded value takes precedence
    // For post-indexed addressing, writeback always occurs (W bit is ignored)
    bool effective_writeback = writeback || !pre_index;
    if (rn != 15 && effective_writeback) {
        if (!(load && effective_writeback && rn == rd)) {
            regs_[rn] = base + adjusted_offset;
        }
    }
}

void Cpu::ExecuteHalfwordDataTransfer(u32 instruction, u32 instruction_address) {
    bool immediate_offset = (instruction >> 22) & 1;
    bool pre_index = (instruction >> 24) & 1;
    bool add_offset = (instruction >> 23) & 1;
    bool writeback = (instruction >> 21) & 1;
    bool load = (instruction >> 20) & 1;
    bool signed_transfer = (instruction >> 6) & 1;
    bool halfword = (instruction >> 5) & 1;
    u32 rn = (instruction >> 16) & 0xF;
    u32 rd = (instruction >> 12) & 0xF;

    // S=0,H=0 is not a halfword/signed transfer operation, and signed stores are not part of the ARM instruction set.
    if ((!signed_transfer && !halfword) || (signed_transfer && !load)) {
        return;
    }

    u32 base = GetRegisterWithPC(rn, instruction_address);
    u32 offset = 0;
    if (immediate_offset) {
        offset = (((instruction >> 8) & 0xF) << 4) | (instruction & 0xF);
    } else {
        u32 rm = instruction & 0xF;
        offset = GetRegisterWithPC(rm, instruction_address);
    }

    u32 adjusted_offset = add_offset ? offset : 0 - offset;
    u32 effective_address = pre_index ? base + adjusted_offset : base;

    // For halfword transfers, align to halfword boundary on GBA (ARM7TDMI behavior)
    u32 aligned_halfword_addr = effective_address & ~1u;

    if (!load) {
        bus_.Write16(aligned_halfword_addr, static_cast<u16>(regs_[rd]));
    } else if (!signed_transfer) {
        regs_[rd] = bus_.Read16(aligned_halfword_addr);
    } else if (halfword) {
        u32 value = bus_.Read16(aligned_halfword_addr);
        regs_[rd] = (value & 0x8000u) ? value | 0xFFFF0000u : value;
    } else {
        u32 value = bus_.Read8(effective_address);  // LDRSB uses byte address
        regs_[rd] = (value & 0x80u) ? value | 0xFFFFFF00u : value;
    }

    // For post-indexed addressing, writeback always occurs (W bit is ignored)
    bool effective_writeback = writeback || !pre_index;
    if (rn != 15 && effective_writeback) {
        regs_[rn] = base + adjusted_offset;
    }
}

void Cpu::ExecuteBlockDataTransfer(u32 instruction, u32 instruction_address) {
    bool pre_index = (instruction >> 24) & 1;
    bool add_offset = (instruction >> 23) & 1;
    bool set_status = (instruction >> 22) & 1;
    bool writeback = (instruction >> 21) & 1;
    bool load = (instruction >> 20) & 1;
    u32 rn = (instruction >> 16) & 0xF;
    u32 register_list = instruction & 0xFFFF;
    bool user_bank_transfer = set_status && !(load && (register_list & (1u << 15)));

    // ARM7TDMI: Rn cannot be R15 (PC) for block data transfer
    if (rn == 15) {
        return;
    }

    u32 base = GetRegisterWithPC(rn, instruction_address);

    // Empty register list handling (ARMv4-v5): load/store R15, adjust base by +/- 0x40
    if (register_list == 0) {
        u32 access_address = add_offset
            ? base + (pre_index ? 4u : 0u)
            : base - 0x40u + (pre_index ? 0u : 4u);

        if (load) {
            u32 loaded_pc = bus_.Read32(access_address);
            if (writeback) {
                regs_[rn] = add_offset ? base + 0x40 : base - 0x40;
            }
            if (set_status) {
                u32* spsr = CurrentSpsr();
                if (spsr != nullptr) {
                    WritePsr(false, *spsr, 0xFFFFFFFFu);
                }
            }
            regs_[15] = loaded_pc & ((cpsr_ & kThumbFlag) ? ~1u : ~3u);
        } else {
            bus_.Write32(access_address & ~3u, instruction_address + 12);
            if (writeback) {
                regs_[rn] = add_offset ? base + 0x40 : base - 0x40;
            }
        }
        return;
    }

    u32 register_count = 0;
    for (u32 reg = 0; reg < 16; ++reg) {
        if (register_list & (1u << reg)) {
            ++register_count;
        }
    }
    // Registers always transfer in ascending number and address order.
    u32 address = add_offset
        ? base + (pre_index ? 4u : 0u)
        : base - register_count * 4 + (pre_index ? 0u : 4u);

    u32 loaded_pc = 0;
    bool pc_loaded = false;
    bool rn_in_list = (register_list & (1u << rn)) != 0;
    bool rn_is_first = rn_in_list &&
        (register_list & ((1u << rn) - 1u)) == 0;
    u32 new_base = add_offset ? base + register_count * 4
                              : base - register_count * 4;
    for (u32 reg = 0; reg < 16; ++reg) {
        if ((register_list & (1u << reg)) == 0) {
            continue;
        }

        if (load) {
            u32 loaded_value;
            if ((address & 3) == 0) {
                loaded_value = bus_.Read32(address);
            } else {
                u32 aligned_addr = address & ~3u;
                u32 word = bus_.Read32(aligned_addr);
                unsigned rotate = (address & 3) * 8;
                loaded_value = (word >> rotate) | (word << (32 - rotate));
            }
            if (reg == 15) {
                loaded_pc = loaded_value;
                pc_loaded = true;
            } else if (user_bank_transfer) {
                SetUserRegister(reg, loaded_value);
            } else {
                regs_[reg] = loaded_value;
            }
        } else {
            // For unaligned word stores in LDM/STM, write to aligned address
            u32 aligned_addr = address & ~3u;
            u32 value = reg == 15 ? instruction_address + 12
                : user_bank_transfer ? GetUserRegister(reg) : regs_[reg];
            if (writeback && !user_bank_transfer && reg == rn && !rn_is_first) {
                value = new_base;
            }
            bus_.Write32(aligned_addr, value);
        }
        address += 4;
    }

    bool effective_writeback = writeback;
    if (effective_writeback && rn != 15) {
        if (!load) {
            regs_[rn] = new_base;
        } else {
            // LDM: ARMv4 behavior - no writeback if Rn in list
            if (!rn_in_list) {
                regs_[rn] = new_base;
            }
        }
    }

    if (pc_loaded) {
        // Write back in the old mode before restoring its saved status and register bank.
        if (set_status) {
            u32* spsr = CurrentSpsr();
            if (spsr != nullptr) {
                WritePsr(false, *spsr, 0xFFFFFFFFu);
            }
        }
        regs_[15] = loaded_pc & ((cpsr_ & kThumbFlag) ? ~1u : ~3u);
    }
}

void Cpu::ExecuteSingleDataSwap(u32 instruction) {
    u32 cond = instruction >> 28;
    if (!CheckCondition(cond, cpsr_)) {
        return;
    }

    bool byte_transfer = (instruction >> 22) & 1;
    u32 rn = (instruction >> 16) & 0xF;
    u32 rd = (instruction >> 12) & 0xF;
    u32 rm = instruction & 0xF;

    if (rn == 15 || rd == 15 || rm == 15) {
        std::fprintf(stderr, "Warning: SWP/SWPB with R15 (PC) is unpredictable\n");
        return;
    }

    u32 base = regs_[rn];
    u32 rm_value = regs_[rm];

    if (byte_transfer) {
        u32 loaded_byte = bus_.Read8(base);
        bus_.Write8(base, static_cast<u8>(rm_value));
        regs_[rd] = loaded_byte;
    } else {
        u32 loaded_word = bus_.Read32(base);
        bus_.Write32(base, rm_value);
        regs_[rd] = loaded_word;
    }
}
