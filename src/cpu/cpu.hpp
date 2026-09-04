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
    
    explicit Cpu(Bus& bus);

    void ExecuteBranch(u32 instruction, u32 instruction_address);
    void ExecuteBranchExchange(u32 instruction);
    void ExecuteSoftwareInterrupt(u32 instruction, u32 instruction_address);
    void ExecuteMultiply(u32 instruction);
    void ExecuteMrs(u32 instruction);
    void ExecuteMsr(u32 instruction, bool immediate);
    void ExecuteDataProcessing(u32 instruction);
    void ExecuteSingleDataTransfer(u32 instruction, u32 instruction_address);
    void ExecuteHalfwordDataTransfer(u32 instruction, u32 instruction_address);
    void ExecuteBlockDataTransfer(u32 instruction, u32 instruction_address);
    void Reset();  // Set up post-BIOS register/mode state.
    void Step();   // Fetch-decode-execute one instruction.

private:
    void SaveBankedRegisters(u32 mode);
    void LoadBankedRegisters(u32 mode);
    u32* CurrentSpsr();
    bool WritePsr(bool write_spsr, u32 value, u32 write_mask);

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

};
