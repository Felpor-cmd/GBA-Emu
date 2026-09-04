#include "cpu.hpp"

#include "cpu_detail.hpp"

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

bool Cpu::WritePsr(bool write_spsr, u32 value, u32 write_mask) {
    u32* psr = write_spsr ? CurrentSpsr() : &cpsr_;
    if (psr == nullptr) {
        return false;
    }

    u32 new_psr = (*psr & ~write_mask) | (value & write_mask);
    if (write_spsr) {
        if ((write_mask & kModeMask) != 0 &&
            !IsValidMode(new_psr & kModeMask)) {
            return false;
        }
        *psr = new_psr;
        return true;
    }

    u32 old_mode = cpsr_ & kModeMask;
    u32 new_mode = new_psr & kModeMask;
    if (!IsValidMode(new_mode)) {
        return false;
    }
    if (old_mode != new_mode) {
        SaveBankedRegisters(old_mode);
        LoadBankedRegisters(new_mode);
    }
    cpsr_ = new_psr;
    return true;
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
        operand = cpu_detail::RotateRight(imm8, rotate_amount);
    } else {
        u32 rm = instruction & 0xF;
        if (rm == 15) {
            return;
        }
        operand = regs_[rm];
    }

    u32 write_mask = PsrWriteMask(field_mask);
    if (!write_spsr && (cpsr_ & kModeMask) == kUserMode) {
        write_mask &= 0xF0000000u;
    }
    if (!write_spsr) {
        write_mask &= ~kThumbFlag;
    }
    WritePsr(write_spsr, operand, write_mask);
}
