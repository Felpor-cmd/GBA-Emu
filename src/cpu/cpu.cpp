#include "cpu.hpp"
#include <cstdio>

Cpu::Cpu(Bus& bus) : bus_(bus) {
    Reset();
}

namespace {

    constexpr u32 kModeMask = 0x1F;
    constexpr u32 kUserMode = 0x10;
    constexpr u32 kFiqMode = 0x11;
    constexpr u32 kIrqMode = 0x12;
    constexpr u32 kSupervisorMode = 0x13;
    constexpr u32 kAbortMode = 0x17;
    constexpr u32 kUndefinedMode = 0x1B;
    constexpr u32 kSystemMode = 0x1F;

    bool IsValidMode(u32 mode) {
        switch (mode) {
            case kUserMode:
            case kFiqMode:
            case kIrqMode:
            case kSupervisorMode:
            case kAbortMode:
            case kUndefinedMode:
            case kSystemMode:
                return true;
            default:
                return false;
        }
    }

    u32 PsrWriteMask(u32 field_mask) {
        u32 mask = 0;
        if (field_mask & 0x8) mask |= 0xFF000000u;
        if (field_mask & 0x4) mask |= 0x00FF0000u;
        if (field_mask & 0x2) mask |= 0x0000FF00u;
        if (field_mask & 0x1) mask |= 0x000000FFu;
        return mask & 0xF00000FFu;
    }

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
                return RotateRight(value, amount);
            default:
                return value;
        }
    }

}  // namespace

void Cpu::SaveBankedRegisters(u32 mode) {
    if (mode == kFiqMode) {
        for (u32 reg = 8; reg <= 14; ++reg) {
            fiq_r8_r14_[reg - 8] = regs_[reg];
        }
        return;
    }

    for (u32 reg = 8; reg <= 12; ++reg) {
        shared_r8_r12_[reg - 8] = regs_[reg];
    }

    std::array<u32, 2>* bank = nullptr;
    switch (mode) {
        case kUserMode:
        case kSystemMode: bank = &user_system_r13_r14_; break;
        case kIrqMode: bank = &irq_r13_r14_; break;
        case kSupervisorMode: bank = &svc_r13_r14_; break;
        case kAbortMode: bank = &abt_r13_r14_; break;
        case kUndefinedMode: bank = &und_r13_r14_; break;
        default: return;
    }
    (*bank)[0] = regs_[13];
    (*bank)[1] = regs_[14];
}

void Cpu::LoadBankedRegisters(u32 mode) {
    if (mode == kFiqMode) {
        for (u32 reg = 8; reg <= 14; ++reg) {
            regs_[reg] = fiq_r8_r14_[reg - 8];
        }
        return;
    }

    for (u32 reg = 8; reg <= 12; ++reg) {
        regs_[reg] = shared_r8_r12_[reg - 8];
    }

    const std::array<u32, 2>* bank = nullptr;
    switch (mode) {
        case kUserMode:
        case kSystemMode: bank = &user_system_r13_r14_; break;
        case kIrqMode: bank = &irq_r13_r14_; break;
        case kSupervisorMode: bank = &svc_r13_r14_; break;
        case kAbortMode: bank = &abt_r13_r14_; break;
        case kUndefinedMode: bank = &und_r13_r14_; break;
        default: return;
    }
    regs_[13] = (*bank)[0];
    regs_[14] = (*bank)[1];
}

bool Cpu::SwitchMode(u32 new_mode) {
    if (!IsValidMode(new_mode)) {
        return false;
    }

    u32 old_mode = cpsr_ & kModeMask;
    if (old_mode == new_mode) {
        return true;
    }

    SaveBankedRegisters(old_mode);
    LoadBankedRegisters(new_mode);
    cpsr_ = (cpsr_ & ~kModeMask) | new_mode;
    return true;
}

u32* Cpu::CurrentSpsr() {
    switch (cpsr_ & kModeMask) {
        case kFiqMode: return &spsr_fiq_;
        case kIrqMode: return &spsr_irq_;
        case kSupervisorMode: return &spsr_svc_;
        case kAbortMode: return &spsr_abt_;
        case kUndefinedMode: return &spsr_und_;
        default: return nullptr;
    }
}

void Cpu::ExecuteMrs(u32 instruction) {
    bool read_spsr = (instruction >> 22) & 1;
    u32 rd = (instruction >> 12) & 0xF;
    if (rd == 15) {
        return;
    }

    if (!read_spsr) {
        regs_[rd] = cpsr_;
        return;
    }

    u32* spsr = CurrentSpsr();
    if (spsr != nullptr) {
        regs_[rd] = *spsr;
    }
}

void Cpu::ExecuteMsr(u32 instruction, bool immediate) {
    bool write_spsr = (instruction >> 22) & 1;
    u32 field_mask = (instruction >> 16) & 0xF;
    u32 operand = 0;

    if (immediate) {
        u32 imm8 = instruction & 0xFF;
        unsigned rotate_amount = ((instruction >> 8) & 0xF) * 2;
        operand = RotateRight(imm8, rotate_amount);
    } else {
        u32 rm = instruction & 0xF;
        if (rm == 15) {
            return;
        }
        operand = regs_[rm];
    }

    u32 write_mask = PsrWriteMask(field_mask);
    if (write_spsr) {
        u32* spsr = CurrentSpsr();
        if (spsr == nullptr) {
            return;
        }

        u32 new_spsr = (*spsr & ~write_mask) | (operand & write_mask);
        if ((write_mask & kModeMask) != 0 &&
            !IsValidMode(new_spsr & kModeMask)) {
            return;
        }
        *spsr = new_spsr;
        return;
    }

    u32 current_mode = cpsr_ & kModeMask;
    if (current_mode == kUserMode) {
        write_mask &= 0xF0000000u;
    }

    u32 new_cpsr = (cpsr_ & ~write_mask) | (operand & write_mask);
    new_cpsr = (new_cpsr & ~kThumbFlag) | (cpsr_ & kThumbFlag);

    u32 new_mode = new_cpsr & kModeMask;
    if (!IsValidMode(new_mode)) {
        return;
    }
    if (!SwitchMode(new_mode)) {
        return;
    }
    cpsr_ = new_cpsr;
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
        cpsr_ = (cpsr_ & ~(kNegativeFlag | kZeroFlag)) |
                (result & kNegativeFlag) |
                (result == 0 ? kZeroFlag : 0);
    }
}

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

void Cpu::ExecuteSingleDataTransfer(u32 instruction, u32 instruction_address) {
    bool register_offset = (instruction >> 25) & 1;
    bool pre_index = (instruction >> 24) & 1;
    bool add_offset = (instruction >> 23) & 1;
    bool byte_transfer = (instruction >> 22) & 1;
    bool writeback = (instruction >> 21) & 1;
    bool load = (instruction >> 20) & 1;
    u32 rn = (instruction >> 16) & 0xF;
    u32 rd = (instruction >> 12) & 0xF;

    u32 base = rn == 15 ? instruction_address + 8 : regs_[rn];
    u32 offset = 0;

    if (register_offset) {
        u32 rm = instruction & 0xF;
        u32 offset_value = rm == 15 ? instruction_address + 8 : regs_[rm];
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
        regs_[rd] = byte_transfer ? bus_.Read8(effective_address)
                                   : bus_.Read32(effective_address);
    } else if (byte_transfer) {
        bus_.Write8(effective_address, static_cast<u8>(regs_[rd]));
    } else {
        bus_.Write32(effective_address, regs_[rd]);
    }

    if (rn != 15 && (!pre_index || writeback)) {
        regs_[rn] = base + adjusted_offset;
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

    // S=0,H=0 is not a halfword/signed transfer operation, and signed stores
    // are not part of the ARM instruction set.
    if ((!signed_transfer && !halfword) || (signed_transfer && !load)) {
        return;
    }

    u32 base = rn == 15 ? instruction_address + 8 : regs_[rn];
    u32 offset = 0;
    if (immediate_offset) {
        offset = (((instruction >> 8) & 0xF) << 4) | (instruction & 0xF);
    } else {
        u32 rm = instruction & 0xF;
        offset = rm == 15 ? instruction_address + 8 : regs_[rm];
    }

    u32 adjusted_offset = add_offset ? offset : 0 - offset;
    u32 effective_address = pre_index ? base + adjusted_offset : base;

    if (!load) {
        bus_.Write16(effective_address, static_cast<u16>(regs_[rd]));
    } else if (!signed_transfer) {
        regs_[rd] = bus_.Read16(effective_address);
    } else if (halfword) {
        u32 value = bus_.Read16(effective_address);
        regs_[rd] = (value & 0x8000u) ? value | 0xFFFF0000u : value;
    } else {
        u32 value = bus_.Read8(effective_address);
        regs_[rd] = (value & 0x80u) ? value | 0xFFFFFF00u : value;
    }

    if (rn != 15 && (!pre_index || writeback)) {
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

    // SPSR restoration for LDM with the S bit is not implemented yet.
    if (set_status || register_list == 0) {
        return;
    }

    u32 register_count = 0;
    for (u32 reg = 0; reg < 16; ++reg) {
        if (register_list & (1u << reg)) {
            ++register_count;
        }
    }

    u32 base = rn == 15 ? instruction_address + 8 : regs_[rn];
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
            regs_[reg] = bus_.Read32(address);
        } else {
            bus_.Write32(address, regs_[reg]);
        }
        address += 4;
    }

    if (writeback && rn != 15) {
        regs_[rn] = add_offset
            ? base + register_count * 4
            : base - register_count * 4;
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
    shared_r8_r12_.fill(0);
    fiq_r8_r14_.fill(0);
    user_system_r13_r14_.fill(0);
    irq_r13_r14_.fill(0);
    svc_r13_r14_.fill(0);
    abt_r13_r14_.fill(0);
    und_r13_r14_.fill(0);
    spsr_fiq_ = 0;
    spsr_irq_ = 0;
    spsr_svc_ = 0;
    spsr_abt_ = 0;
    spsr_und_ = 0;
    regs_[13] = 0x03007F00;  // SP: matches the post-BIOS stack pointer (GBATEK: BIOS).
    user_system_r13_r14_[0] = regs_[13];
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

    // MUL and MLA are also encoded in the ARM data-processing space.
    bool is_multiply = ((instruction & 0x0FC000F0u) == 0x00000090u);

    bool is_mrs = ((instruction & 0x0FBF0FFFu) == 0x010F0000u);
    bool is_msr_register =
        ((instruction & 0x0FB0FFF0u) == 0x0120F000u);
    bool is_msr_immediate =
        ((instruction & 0x0FB0F000u) == 0x0320F000u);

    if (is_branch_exchange) {
        ExecuteBranchExchange(instruction);
        return;
    }

    if (is_multiply) {
        ExecuteMultiply(instruction);
        return;
    }

    if (is_mrs) {
        ExecuteMrs(instruction);
        return;
    }

    if (is_msr_register) {
        ExecuteMsr(instruction, false);
        return;
    }

    if (is_msr_immediate) {
        ExecuteMsr(instruction, true);
        return;
    }

    if(is_branch) {
        ExecuteBranch(instruction, instruction_address);
        return;
    }

    bool is_halfword_transfer =
        (instruction & 0x0E000000u) == 0 &&
        (instruction & 0x00000090u) == 0x00000090u &&
        (instruction & 0x00000060u) != 0;

    if (is_halfword_transfer) {
        ExecuteHalfwordDataTransfer(instruction, instruction_address);
        return;
    }

    bool is_block_data_transfer = ((instruction >> 25) & 0x7) == 0b100;

    if (is_block_data_transfer) {
        ExecuteBlockDataTransfer(instruction, instruction_address);
        return;
    }

    u32 category = (instruction >> 26) & 0b11;

    if(category == 0b00) {
        ExecuteDataProcessing(instruction);
    } else if (category == 0b01) {
        ExecuteSingleDataTransfer(instruction, instruction_address);
    }
}
