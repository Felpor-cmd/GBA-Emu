#pragma once

#include <array>
#include <ostream>
#include "cpu/cpu.hpp"
#include "ppu/ppu.hpp"

class Emulator {
public:
    explicit Emulator(std::vector<u8> rom, bool run_cpu = true, std::vector<u8> bios = {})
        : bus_(std::move(rom), std::move(bios)), cpu_(bus_, true), ppu_(bus_), run_cpu_(run_cpu) {
        if (bus_.HasBios()) cpu_.PowerOn();
    }
    void RunFrame(bool render = true);
    void RenderFrame();
    void DumpFrame(const char* path) const;
    void PrintTrace(std::ostream& out) const;
    Bus& Memory() { return bus_; }
    const Ppu::Frame& Pixels() const { return ppu_.Pixels(); }
    u64 Frames() const { return frames_; }
    u32 Pc() const { return cpu_.GetRegister(15); }
    u64 Swi12Calls() const { return cpu_.Swi12Calls(); }
    struct FrameTiming { double hardware_ms = 0, render_ms = 0; };
    void EnableTimingDiagnostics() { timing_diagnostics_ = true; }
    FrameTiming LastFrameTiming() const { return frame_timing_; }

private:
    struct Trace { u32 pc, opcode, cpsr, sp, lr, stack; };
    Bus bus_;
    Cpu cpu_;
    Ppu ppu_;
    std::array<Trace, 64> trace_{};
    u64 trace_count_ = 0, frames_ = 0;
    s64 cycle_debt_ = 0;
    bool run_cpu_ = true;
    bool timing_diagnostics_ = false;
    FrameTiming frame_timing_{};
};
