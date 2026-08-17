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

// Owns every RAM region plus the loaded ROM, and will become the single
// place every other component (CPU, PPU, timers, DMA) goes through to touch
// memory. Right now the read/write methods are stubs -- wiring them up to
// the real address ranges is the next milestone after this compiles.
class Bus {
public:
    explicit Bus(std::vector<u8> rom);

    u8  Read8(u32 address) const;
    u16 Read16(u32 address) const;
    u32 Read32(u32 address) const;

    void Write8(u32 address, u8 value);
    void Write16(u32 address, u16 value);
    void Write32(u32 address, u32 value);

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

    std::array<u8, kIoSize> io_{};
};
