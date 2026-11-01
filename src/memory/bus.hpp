#pragma once
#include <array>
#include <vector>
#include "../types.hpp"

// Region sizes from the GBA memory map (GBATEK: GBA Memory Map).
constexpr u32 kBiosSize    = 0x4000;    // 16 KB  BIOS ROM
constexpr u32 kEwramSize   = 0x40000;   // 256 KB on-board WRAM
constexpr u32 kIwramSize   = 0x8000;    // 32 KB  in-chip WRAM
constexpr u32 kPaletteSize = 0x400;     // 1 KB   BG/OBJ palette RAM
constexpr u32 kVramSize    = 0x18000;   // 96 KB  video RAM
constexpr u32 kOamSize     = 0x400;     // 1 KB   object attribute memory
constexpr u32 kSramSize    = 0x8000;   // 32 KB  static RAM

constexpr u32 kIoSize      = 0x400;     // 1 KB   I/O Registers - PLACEHOLDER

// Owns RAM, I/O storage, BIOS data, and the loaded ROM. CPU and PPU access
// memory through this shared address map.
class Bus {
public:
    explicit Bus(std::vector<u8> rom, std::vector<u8> bios = {});

    u8  Read8(u32 address) const;
    u16 Read16(u32 address) const;
    u32 Read32(u32 address) const;

    void Write8(u32 address, u8 value);
    void Write16(u32 address, u16 value);
    void Write32(u32 address, u32 value);

    // Number of 16.78 MHz bus clocks for one CPU access. `sequential` is
    // supplied by the CPU's instruction/data access schedule.
    u32 AccessCycles(u32 address, unsigned width, bool sequential) const;
    bool PrefetchEnabled() const { return (waitcnt_ & (1u << 14)) != 0; }
    u16 GetWaitcnt() const { return waitcnt_; }
    void SetVideoTiming(u16 scanline, bool hblank);
    // `pressed` uses one bits for pressed GBA buttons; KEYINPUT reads active-low.
    void SetKeyInput(u16 pressed);
    void ResetMemory(u32 flags);
    u32 RunDma(unsigned timing);
    void AdvanceTimers(u32 cycles);
    void RequestInterrupt(u16 bits);
    bool InterruptPending() const { return (Read16(0x04000200) & Read16(0x04000202)) != 0; }
    bool IrqEnabled() const { return (Read16(0x04000208) & 1u) && InterruptPending(); }
    bool HasBios() const { return has_bios_; }

private:
    std::array<u8, kBiosSize>    bios_{};
    std::array<u8, kEwramSize>   ewram_{};
    std::array<u8, kIwramSize>   iwram_{};
    std::array<u8, kPaletteSize> palette_{};
    std::array<u8, kVramSize>    vram_{};
    std::array<u8, kOamSize>     oam_{};
    std::array<u8, kIoSize>      io_{};
    std::array<u8, kSramSize>    sram_{};
    std::vector<u8>              rom_;  
    u16 waitcnt_ = 0;
    bool has_bios_ = false;
    struct Dma { u32 source = 0, destination = 0, reload = 0; bool enabled = false; };
    std::array<Dma, 4> dma_{};
    struct Timer {
        u16 reload = 0;
        u16 counter = 0;
        u8 control = 0;
        u16 prescaler_phase = 0;
    };
    std::array<Timer, 4> timers_{};
    u64 IncrementTimer(unsigned timer, u64 ticks);
};
