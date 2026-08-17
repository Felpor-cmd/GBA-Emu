#pragma once
#include <array>
#include "../memory/bus.hpp"
#include "../types.hpp"

// The ARM7TDMI core. Holds the register file and will grow to hold banked
// registers (for FIQ/IRQ/SVC/etc. modes) once mode switching is implemented.

constexpr u32 kThumbFlag = 0x20; // CPSR bit 5 — the "T" bit

class Cpu {
    public:
    explicit Cpu(Bus& bus);

    void Reset();  // Set up post-BIOS register/mode state.
    void Step();   // Fetch-decode-execute one instruction. Empty for now.

private:
    std::array<u32, 16> regs_{};  // r0-r15, where r15 is the program counter.
    u32 cpsr_ = 0;                // Current Program Status Register (flags + mode).

    Bus& bus_;

};
