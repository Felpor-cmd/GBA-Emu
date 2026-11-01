#include "emulator.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <stdexcept>

void Emulator::RunFrame() {
    cycle_debt_ += Ppu::kCyclesPerFrame;
    while (cycle_debt_ > 0) {
        const u32 pc = cpu_.GetRegister(15), cpsr = cpu_.GetCpsr();
        const u32 sp = cpu_.GetRegister(13);
        if (run_cpu_ && !cpu_.IsHalted()) {
            trace_[trace_count_++ % trace_.size()] = {pc,
                (cpsr & kThumbFlag) ? bus_.Read16(pc) : bus_.Read32(pc), cpsr,
                sp, cpu_.GetRegister(14), bus_.Read32(sp)};
        }
        u32 remaining = run_cpu_ ? cpu_.Step() + bus_.RunDma(0) : ppu_.CyclesToEvent();
        while (remaining) {
            const u32 elapsed = std::min(remaining, ppu_.CyclesToEvent());
            const u32 events = ppu_.Advance(elapsed);
            remaining -= elapsed;
            cycle_debt_ -= elapsed;
            if (events & 1u) remaining += bus_.RunDma(1);
            if (events & 2u) remaining += bus_.RunDma(2);
        }
    }
    ppu_.RenderFrame();
    ++frames_;
}

void Emulator::DumpFrame(const char* path) const {
    std::ofstream output(path, std::ios::binary);
    output << "P6\n240 160\n255\n";
    for (u32 color : ppu_.Pixels()) {
        const char rgb[] = {static_cast<char>(color >> 16), static_cast<char>(color >> 8),
                            static_cast<char>(color)};
        output.write(rgb, 3);
    }
    if (!output) throw std::runtime_error("Cannot write frame capture");
}

void Emulator::PrintTrace(std::ostream& out) const {
    out << std::hex << std::setfill('0');
    for (int reg = 0; reg < 16; ++reg)
        out << "r" << std::dec << reg << "=" << std::hex << std::setw(8) << cpu_.GetRegister(reg)
            << (reg % 4 == 3 ? '\n' : ' ');
    const u64 begin = trace_count_ - std::min<u64>(trace_count_, trace_.size());
    for (u64 i = begin; i < trace_count_; ++i) {
        const auto& t = trace_[i % trace_.size()];
        out << "pc=" << std::setw(8) << t.pc << " op=" << std::setw(8) << t.opcode
            << " cpsr=" << std::setw(8) << t.cpsr << " sp=" << std::setw(8) << t.sp
            << " lr=" << std::setw(8) << t.lr << " [sp]=" << std::setw(8) << t.stack << '\n';
    }
}
