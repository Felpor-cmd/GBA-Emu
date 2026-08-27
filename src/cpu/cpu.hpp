#pragma once
#include <array>
#include "../memory/bus.hpp"
#include "../types.hpp"

// The ARM7TDMI core. Holds the register file and will grow to hold banked
// registers (for FIQ/IRQ/SVC/etc. modes) once mode switching is implemented.

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
    void ExecuteMultiply(u32 instruction);
    void ExecuteDataProcessing(u32 instruction);
    void ExecuteSingleDataTransfer(u32 instruction, u32 instruction_address);
    void ExecuteHalfwordDataTransfer(u32 instruction, u32 instruction_address);
    void ExecuteBlockDataTransfer(u32 instruction, u32 instruction_address);
    void Reset();  // Set up post-BIOS register/mode state.
    void Step();   // Fetch-decode-execute one instruction. Empty for now.

private:
    std::array<u32, 16> regs_{};  // r0-r15, where r15 is the program counter.
    u32 cpsr_ = 0;                // Current Program Status Register (flags + mode).


    Bus& bus_;

};
