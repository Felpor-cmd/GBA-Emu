#include "cpu.hpp"

namespace {
enum class ArmKind { Undefined, Branch, BranchExchange, Multiply, LongMultiply,
                     Mrs, MsrRegister, MsrImmediate, SoftwareInterrupt, Swap,
                     Halfword, Block, DataProcessing, SingleData };

ArmKind ClassifyArm(u32 insn) {
    if ((insn & 0x0F000000u) == 0x0F000000u) return ArmKind::SoftwareInterrupt;
    if ((insn & 0x0E000000u) == 0x0C000000u) return ArmKind::Undefined;
    if ((insn & 0x0E000000u) == 0x0A000000u) return ArmKind::Branch;
    if ((insn & 0x0E000000u) == 0x08000000u) return ArmKind::Block;
    if ((insn & 0x0C000000u) == 0x04000000u) {
        return ((insn & 0x02000010u) == 0x02000010u)
            ? ArmKind::Undefined : ArmKind::SingleData;
    }
    if ((insn & 0x0C000000u) != 0) return ArmKind::Undefined;

    if ((insn & 0x0FFFFFF0u) == 0x012FFF10u) return ArmKind::BranchExchange;
    // BLX register, CLZ and BKPT are ARMv5 extensions in ARMv4's reserved space.
    if ((insn & 0x0FFFFFF0u) == 0x012FFF30u ||
        (insn & 0x0FFF0FF0u) == 0x016F0F10u ||
        (insn & 0x0FF000F0u) == 0x01200070u) return ArmKind::Undefined;
    if ((insn & 0x0FC000F0u) == 0x00000090u) return ArmKind::Multiply;
    if ((insn & 0x0F8000F0u) == 0x00800090u) return ArmKind::LongMultiply;
    if ((insn & 0x0FBF0FFFu) == 0x010F0000u) return ArmKind::Mrs;
    if ((insn & 0x0FB0FFF0u) == 0x0120F000u) return ArmKind::MsrRegister;
    if ((insn & 0x0FB0F000u) == 0x0320F000u) return ArmKind::MsrImmediate;
    if ((insn & 0x0FB00FF0u) == 0x01000090u) return ArmKind::Swap;
    if ((insn & 0x0E000090u) == 0x00000090u) {
        const bool load = (insn & (1u << 20)) != 0;
        const u32 sh = (insn >> 5) & 3u;
        // S/H=10,11 with L=0 are ARMv5 doubleword transfers.
        if (sh == 0 || (!load && sh != 1)) return ArmKind::Undefined;
        if ((insn & (1u << 22)) == 0 && (insn & 0xF00u) != 0)
            return ArmKind::Undefined;
        return ArmKind::Halfword;
    }
    // Register-shift encodings require bit 7=0. The test/compare opcode
    // slots without S are reserved (including ARMv5 DSP encodings).
    if ((insn & 0x02000090u) == 0x00000090u) return ArmKind::Undefined;
    const u32 opcode = (insn >> 21) & 15u;
    if (opcode >= 8 && opcode <= 11 && (insn & (1u << 20)) == 0)
        return ArmKind::Undefined;
    return ArmKind::DataProcessing;
}
}  // namespace

Cpu::Cpu(Bus& bus, bool emulate_bios) : bus_(bus), emulate_bios_(emulate_bios) {
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

u32 Cpu::GetRegisterWithPC(int index, u32 instruction_address) const {
    if (index == 15) {
        return instruction_address + 8;
    }
    return regs_[static_cast<size_t>(index)];
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
    irq_r13_r14_[0] = 0x03007FA0u;
    svc_r13_r14_[0] = 0x03007FE0u;
    halted_ = hle_irq_active_ = false;
    bios_wait_mask_ = 0;
    user_system_r13_r14_[0] = regs_[13];
    regs_[15] = 0x08000000;  // instruction_address: cartridge ROM entry point.
    cpsr_ = 0x1F;             // System mode, ARM state, IRQ/FIQ unmasked.
    cycles_ = step_cycles_ = prefetch_clocks_ = prefetch_halfwords_ = 0;
    next_fetch_sequential_ = data_sequential_ = step_from_rom_ = false;
    thumb_bl_pending_ = false;
    thumb_bl_next_address_ = 0;
    last_waitcnt_ = bus_.GetWaitcnt();
}

u32 Cpu::ChargeFetch(u32 address, unsigned width, bool sequential) {
    const u32 region = address >> 24;
    const bool rom = region >= 8 && region <= 13;
    const u32 units = width == 4 ? 2u : 1u;
    if (!rom || !bus_.PrefetchEnabled() || !sequential ||
        (address & 0x1FFFFu) == 0) {
        prefetch_halfwords_ = prefetch_clocks_ = 0;
    } else if (sequential && prefetch_halfwords_ >= units) {
        prefetch_halfwords_ -= units;
        return 1;
    }
    prefetch_halfwords_ = prefetch_clocks_ = 0;
    return bus_.AccessCycles(address, width, sequential);
}

void Cpu::ChargeInternal(u32 clocks) {
    step_cycles_ += clocks;
    if (!step_from_rom_ || !bus_.PrefetchEnabled()) return;
    const u32 next_address = regs_[15];
    const u32 halfword_cost = bus_.AccessCycles(next_address, 2, true);
    prefetch_clocks_ += clocks;
    while (prefetch_clocks_ >= halfword_cost && prefetch_halfwords_ < 8) {
        prefetch_clocks_ -= halfword_cost;
        ++prefetch_halfwords_;
    }
    if (prefetch_halfwords_ == 8) prefetch_clocks_ = 0;
}

void Cpu::ChargeData(u32 address, unsigned width) {
    const u32 clocks = bus_.AccessCycles(address, width, data_sequential_);
    step_cycles_ += clocks;
    data_sequential_ = true;
    next_fetch_sequential_ = false;
    if (address >> 24 >= 8 && address >> 24 <= 13) {
        prefetch_halfwords_ = prefetch_clocks_ = 0;
    } else {
        // The Game Pak bus can fill the prefetch buffer during internal RAM/I/O access.
        if (step_from_rom_ && bus_.PrefetchEnabled()) {
            const u32 cost = bus_.AccessCycles(regs_[15], 2, true);
            prefetch_clocks_ += clocks;
            while (prefetch_clocks_ >= cost && prefetch_halfwords_ < 8) {
                prefetch_clocks_ -= cost;
                ++prefetch_halfwords_;
            }
            if (prefetch_halfwords_ == 8) prefetch_clocks_ = 0;
        }
    }
}

u8 Cpu::ReadData8(u32 a) { ChargeData(a, 1); return bus_.Read8(a); }
u16 Cpu::ReadData16(u32 a) { ChargeData(a, 2); return bus_.Read16(a); }
u32 Cpu::ReadData32(u32 a) { ChargeData(a, 4); return bus_.Read32(a); }
void Cpu::WriteData8(u32 a, u8 v) { ChargeData(a, 1); bus_.Write8(a, v); }
void Cpu::WriteData16(u32 a, u16 v) { ChargeData(a, 2); bus_.Write16(a, v); }
void Cpu::WriteData32(u32 a, u32 v) { ChargeData(a, 4); bus_.Write32(a, v); }

void Cpu::ChargeRefill(u32 target, unsigned width) {
    prefetch_halfwords_ = prefetch_clocks_ = 0;
    step_cycles_ += bus_.AccessCycles(target, width, false);
    step_cycles_ += bus_.AccessCycles(target + width, width, true);
    next_fetch_sequential_ = true;
}

u32 Cpu::Step() {
    return StepUntilEvent(4);
}

u32 Cpu::StepUntilEvent(u32 halt_cycles) {
    step_cycles_ = 0;
    data_sequential_ = false;
    if (ServiceInterruptBoundary()) {
        cycles_ += step_cycles_;
        return step_cycles_;
    }
    if (halted_) {
        const u32 elapsed = halt_cycles ? halt_cycles : 1;
        cycles_ += elapsed;
        return elapsed;
    }
    if (last_waitcnt_ != bus_.GetWaitcnt()) {
        prefetch_halfwords_ = prefetch_clocks_ = 0;
        last_waitcnt_ = bus_.GetWaitcnt();
    }
    bool thumb_mode = (cpsr_ & kThumbFlag) != 0;
    u32 instruction_address = regs_[15];
    step_from_rom_ = (instruction_address >> 24) >= 8 &&
                     (instruction_address >> 24) <= 13;
    const unsigned width = thumb_mode ? 2 : 4;
    step_cycles_ += ChargeFetch(instruction_address, width, next_fetch_sequential_);
    next_fetch_sequential_ = true;

    if (thumb_mode) {
        u16 instruction = bus_.Read16(instruction_address);
        regs_[15] = instruction_address + 2;
        if (ExecuteThumb(instruction, instruction_address))
            ChargeRefill(regs_[15], (cpsr_ & kThumbFlag) ? 2 : 4);
        cycles_ += step_cycles_;
        return step_cycles_;
    }

    u32 instruction = bus_.Read32(instruction_address);
    thumb_bl_pending_ = false;
    regs_[15] = instruction_address + 4;
    u32 condition = instruction >> 28;
    if (condition != 0xFu && !CheckCondition(condition, cpsr_)) {
        cycles_ += step_cycles_;
        return step_cycles_;
    }
    const ArmKind kind = condition == 0xFu ? ArmKind::Undefined : ClassifyArm(instruction);
    const u32 multiplier = (kind == ArmKind::Multiply || kind == ArmKind::LongMultiply)
        ? regs_[(instruction >> 8) & 15u] : 0;
    switch (kind) {
        case ArmKind::Undefined: EnterUndefinedInstructionException(instruction_address); break;
        case ArmKind::SoftwareInterrupt: ExecuteSoftwareInterrupt(instruction, instruction_address); break;
        case ArmKind::BranchExchange: ExecuteBranchExchange(instruction); break;
        case ArmKind::Multiply: ExecuteMultiply(instruction); break;
        case ArmKind::LongMultiply: ExecuteLongMultiply(instruction); break;
        case ArmKind::Mrs: ExecuteMrs(instruction); break;
        case ArmKind::MsrRegister: ExecuteMsr(instruction, false); break;
        case ArmKind::MsrImmediate: ExecuteMsr(instruction, true); break;
        case ArmKind::Branch: ExecuteBranch(instruction, instruction_address); break;
        case ArmKind::Swap: ExecuteSingleDataSwap(instruction); break;
        case ArmKind::Halfword: ExecuteHalfwordDataTransfer(instruction, instruction_address); break;
        case ArmKind::Block: ExecuteBlockDataTransfer(instruction, instruction_address); break;
        case ArmKind::DataProcessing: ExecuteDataProcessing(instruction, instruction_address); break;
        case ArmKind::SingleData: ExecuteSingleDataTransfer(instruction, instruction_address); break;
    }

    if (kind == ArmKind::Multiply || kind == ArmKind::LongMultiply) {
        const u32 rs = multiplier;
        const bool signed_short = static_cast<s32>(rs) >= -128 && static_cast<s32>(rs) <= 127;
        const bool signed_medium = static_cast<s32>(rs) >= -32768 && static_cast<s32>(rs) <= 32767;
        const bool signed_long = static_cast<s32>(rs) >= -8388608 && static_cast<s32>(rs) <= 8388607;
        const bool unsigned_long = kind == ArmKind::LongMultiply &&
                                   (instruction & (1u << 22)) == 0;
        const u32 m = unsigned_long
            ? (rs <= 0xFFu ? 1 : rs <= 0xFFFFu ? 2 : rs <= 0xFFFFFFu ? 3 : 4)
            : (signed_short ? 1 : signed_medium ? 2 : signed_long ? 3 : 4);
        ChargeInternal(m + ((instruction >> 21) & 1u) + (kind == ArmKind::LongMultiply));
    } else if (kind == ArmKind::DataProcessing && !(instruction & (1u << 25)) &&
               (instruction & (1u << 4))) {
        ChargeInternal(1);
    } else if (kind == ArmKind::SingleData || kind == ArmKind::Halfword || kind == ArmKind::Block || kind == ArmKind::Swap) {
        if ((kind == ArmKind::SingleData || kind == ArmKind::Halfword || kind == ArmKind::Block) &&
            (instruction & (1u << 20))) ChargeInternal(1);
        if (kind == ArmKind::Swap) ChargeInternal(1);
    }
    if (regs_[15] != instruction_address + 4) {
        ChargeRefill(regs_[15], (cpsr_ & kThumbFlag) ? 2 : 4);
    }
    cycles_ += step_cycles_;
    return step_cycles_;
}
