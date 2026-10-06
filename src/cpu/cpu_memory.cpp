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

    if (register_list == 0) {
        return;
    }

    u32 register_count = 0;
    for (u32 reg = 0; reg < 16; ++reg) {
        if (register_list & (1u << reg)) {
            ++register_count;
        }
    }

    u32 base = GetRegisterWithPC(rn, instruction_address);
    u32 address = 0;
    if (add_offset) {
        address = pre_index ? base + 4 : base;
    } else {
        address = pre_index
            ? base - register_count * 4
            : base - (register_count - 1) * 4;
    }

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
                regs_[15] = loaded_value & ~3u;
            } else {
                regs_[reg] = loaded_value;
            }
        } else {
            // For unaligned word stores in LDM/STM, write to aligned address
            u32 aligned_addr = address & ~3u;
            bus_.Write32(aligned_addr, regs_[reg]);
        }
        address += 4;
    }

    // Exception return: if S bit is set and PC was in register list, restore CPSR from SPSR
    if (load && set_status && (register_list & (1u << 15))) {
        u32* spsr = CurrentSpsr();
        if (spsr != nullptr) {
            u32 spsr_value = *spsr;
            WritePsr(false, spsr_value, 0xFFFFFFFFu);
        }
    }

    // For post-indexed addressing, writeback always occurs (W bit is ignored)
    bool effective_writeback = writeback || !pre_index;
    if (effective_writeback && rn != 15) {
        regs_[rn] = add_offset
            ? base + register_count * 4
            : base - register_count * 4;
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
