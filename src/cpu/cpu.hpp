#pragma once
#include <array>
#include "../memory/bus.hpp"
#include "../types.hpp"

// The ARM7TDMI core. The active register view is backed by mode-specific banks
// for exception handling and SPSR support.

constexpr u32 kThumbFlag = 0x20; // CPSR bit 5 — the "T" bit

class Cpu {
    public:
    static bool CheckCondition(u32 cond, u32 cpsr);
    
    u32 GetRegister(int index) const { return regs_[static_cast<size_t>(index)]; }
    void SetRegister(int index, u32 value) { regs_[static_cast<size_t>(index)] = value; }
    u32 GetCpsr() const { return cpsr_; }
    bool SetCpsr(u32 value);
    bool SetSpsr(u32 value);
    
    explicit Cpu(Bus& bus, bool emulate_bios = false);
    bool IsHalted() const { return halted_; }
    void Wake() { halted_ = false; }

    void ExecuteBranch(u32 instruction, u32 instruction_address);
    void ExecuteBranchExchange(u32 instruction);
    void ExecuteSoftwareInterrupt(u32 instruction, u32 instruction_address);
    void ExecuteMultiply(u32 instruction);
    void ExecuteLongMultiply(u32 instruction);
    void ExecuteMrs(u32 instruction);
    void ExecuteMsr(u32 instruction, bool immediate);
    void ExecuteDataProcessing(u32 instruction, u32 instruction_address);
    void ExecuteSingleDataTransfer(u32 instruction, u32 instruction_address);
    void ExecuteHalfwordDataTransfer(u32 instruction, u32 instruction_address);
    void ExecuteSingleDataSwap(u32 instruction);
    void ExecuteBlockDataTransfer(u32 instruction, u32 instruction_address);
    void EnterUndefinedInstructionException(u32 instruction_address);
    void EnterSoftwareInterruptException(u32 instruction_address);
    void Reset();  // Set up post-BIOS register/mode state.
    u32 Step();    // Fetch-decode-execute one instruction; return elapsed clocks.
    // A halted CPU can sleep until the coordinator's next hardware event.
    u32 StepUntilEvent(u32 halt_cycles);
    u64 GetCycles() const { return cycles_; }

private:
    bool ExecuteThumb(u16 instruction, u32 instruction_address);
    void ExecuteThumbMoveShiftedRegister(u16 instruction);
    void ExecuteThumbBranchExchange(u16 instruction);
    void SaveBankedRegisters(u32 mode);
    void LoadBankedRegisters(u32 mode);
    u32* CurrentSpsr();
    bool WritePsr(bool write_spsr, u32 value, u32 write_mask);
    u32 GetRegisterWithPC(int index, u32 instruction_address) const;
    u32 GetUserRegister(u32 index) const;
    void SetUserRegister(u32 index, u32 value);
    u8 ReadData8(u32 address);
    u16 ReadData16(u32 address);
    u32 ReadData32(u32 address);
    void WriteData8(u32 address, u8 value);
    void WriteData16(u32 address, u16 value);
    void WriteData32(u32 address, u32 value);
    void ChargeData(u32 address, unsigned width);
    void ChargeInternal(u32 cycles);
    u32 ChargeFetch(u32 address, unsigned width, bool sequential);
    void ChargeRefill(u32 target, unsigned width);
    bool ServiceInterruptBoundary();

    std::array<u32, 16> regs_{};  // r0-r15, where r15 is the program counter.
    u32 cpsr_ = 0;                // Current Program Status Register (flags + mode).

    std::array<u32, 5> shared_r8_r12_{};
    std::array<u32, 7> fiq_r8_r14_{};
    std::array<u32, 2> user_system_r13_r14_{};
    std::array<u32, 2> irq_r13_r14_{};
    std::array<u32, 2> svc_r13_r14_{};
    std::array<u32, 2> abt_r13_r14_{};
    std::array<u32, 2> und_r13_r14_{};

    u32 spsr_fiq_ = 0;
    u32 spsr_irq_ = 0;
    u32 spsr_svc_ = 0;
    u32 spsr_abt_ = 0;
    u32 spsr_und_ = 0;

    Bus& bus_;
    bool emulate_bios_ = false;
    bool halted_ = false;
    u16 bios_wait_mask_ = 0;
    bool hle_irq_active_ = false;
    u64 cycles_ = 0;
    u32 step_cycles_ = 0;
    u32 prefetch_clocks_ = 0;
    u32 prefetch_halfwords_ = 0;
    bool next_fetch_sequential_ = false;
    bool data_sequential_ = false;
    bool step_from_rom_ = false;
    bool thumb_bl_pending_ = false;
    u32 thumb_bl_next_address_ = 0;
    u16 last_waitcnt_ = 0;

};
