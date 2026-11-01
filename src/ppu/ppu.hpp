#pragma once

#include <array>

#include "memory/bus.hpp"

class Ppu {
public:
    static constexpr int kWidth = 240;
    static constexpr int kHeight = 160;
    static constexpr u32 kCyclesPerFrame = 280896;
    using Frame = std::array<u32, kWidth * kHeight>;

    explicit Ppu(Bus& bus) : bus_(bus) { bus_.SetVideoTiming(0, false); }
    u32 Advance(u32 cycles);
    u32 CyclesToEvent() const { return (line_cycles_ < 1006 ? 1006u : 1232u) - line_cycles_; }
    void RenderFrame();
    const Frame& Pixels() const { return pixels_; }

private:
    static u32 ToRgba(u16 color);
    Bus& bus_;
    Frame pixels_{};
    u32 line_cycles_ = 0;
    u16 scanline_ = 0;
};
