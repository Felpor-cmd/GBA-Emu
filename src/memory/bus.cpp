#include "bus.hpp"

#include <algorithm>

Bus::Bus(std::vector<u8> rom, std::vector<u8> bios) : rom_(std::move(rom)) {
    std::copy_n(bios.begin(), std::min(bios.size(), bios_.size()), bios_.begin());
}

u8 Bus::Read8(u32 address) const {
    switch (address >> 24){
        case 0x00: return bios_[address & (kBiosSize - 1)];

        case 0x04:
            if (address == 0x04000204) return static_cast<u8>(waitcnt_);
            if (address == 0x04000205) return static_cast<u8>(waitcnt_ >> 8);
            return io_[address & (kIoSize -1)];

        case 0x02: return ewram_[address & (kEwramSize - 1)];
        case 0x03: return iwram_[address & (kIwramSize - 1)];
        case 0x05: return palette_[address & (kPaletteSize - 1)];
        case 0x06: return vram_[address & (kVramSize - 1)];
        case 0x07: return oam_[address & (kOamSize - 1)];

        case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: {
            u32 offset = address & 0x01FFFFFF;    
            if (offset >= rom_.size())
                return 0; // Out of bounds
            else
                return rom_[offset]; // ROM is read-only, return value from ROM
        }

        case 0x0E: return sram_[address & (kSramSize - 1)];

        default: return 0;
    }
}

u16 Bus::Read16(u32 address) const {
    return static_cast<u16>(Read8(address)) |
           (static_cast<u16>(Read8(address + 1)) << 8);
}

u32 Bus::Read32(u32 address) const {
    return static_cast<u32>(Read16(address)) |
           (static_cast<u32>(Read16(address + 2)) << 16);
}

void Bus::Write8(u32 address, u8 value) {
    switch (address >> 24){
        case 0x00: break; // BIOS is read-only, ignore writes

        case 0x04:
            if (address == 0x04000204) {
                waitcnt_ = (waitcnt_ & 0xFF00u) | value;
            } else if (address == 0x04000205) {
                waitcnt_ = (waitcnt_ & 0x00FFu) | (static_cast<u16>(value & 0x7Fu) << 8);
            } else {
                io_[address & (kIoSize - 1)] = value;
            }
            break;

        case 0x02: ewram_[address & (kEwramSize - 1)] = value; break;
        case 0x03: iwram_[address & (kIwramSize - 1)] = value; break;
        case 0x05: palette_[address & (kPaletteSize - 1)] = value; break;
        case 0x06: vram_[address & (kVramSize - 1)] = value; break;
        case 0x07: oam_[address & (kOamSize - 1)] = value; break;
        
        case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: {
            // ROM is read-only, ignore writes
            break;
        }
        
        case 0x0E: sram_[address & (kSramSize - 1)] = value; break;

        default: break;
    }
}

void Bus::Write16(u32 address, u16 value) {
    Write8(address, static_cast<u8>(value & 0xFF));
    Write8(address + 1, static_cast<u8>((value >> 8) & 0xFF));
}

void Bus::Write32(u32 address, u32 value) {
    Write16(address, static_cast<u16>(value & 0xFFFF));
    Write16(address + 2, static_cast<u16>((value >> 16) & 0xFFFF));
}

void Bus::SetVideoTiming(u16 scanline, bool hblank) {
    const u16 control = Read16(0x04000004u);
    const u16 status = static_cast<u16>((control & ~7u) |
        (scanline >= 160 && scanline < 227 ? 1u : 0u) | (hblank ? 2u : 0u) |
        (scanline == (control >> 8) ? 4u : 0u));
    io_[4] = static_cast<u8>(status);
    io_[5] = static_cast<u8>(status >> 8);
    io_[6] = static_cast<u8>(scanline);
    io_[7] = 0;
}

void Bus::ResetMemory(u32 flags) {
    if (flags & (1u << 0)) ewram_.fill(0);
    if (flags & (1u << 1)) iwram_.fill(0);
    if (flags & (1u << 2)) palette_.fill(0);
    if (flags & (1u << 3)) vram_.fill(0);
    if (flags & (1u << 4)) oam_.fill(0);
}

u32 Bus::AccessCycles(u32 address, unsigned width, bool sequential) const {
    const u32 region = address >> 24;
    const u32 units = width == 4 ? 2u : 1u;
    if (region == 0x02) return 3u * units;
    if (region >= 0x04 && region <= 0x07) return units;
    if (region >= 0x08 && region <= 0x0D) {
        const unsigned bank = (region - 0x08) / 2;
        const unsigned shift = 2 + bank * 3;
        constexpr u32 first[4] = {4, 3, 2, 8};
        const u32 nonsequential = first[(waitcnt_ >> shift) & 3u];
        const bool fast = (waitcnt_ >> (shift + 2)) & 1u;
        const u32 sequential_wait = bank == 0 ? (fast ? 1u : 2u)
                                  : bank == 1 ? (fast ? 1u : 4u)
                                              : (fast ? 1u : 8u);
        // Game Pak has a 16-bit data bus. A word uses a second S access.
        const bool boundary = (address & 0x1FFFFu) == 0;
        return 1u + ((sequential && !boundary) ? sequential_wait : nonsequential)
               + (units == 2 ? 1u + sequential_wait : 0u);
    }
    if (region == 0x0E) {
        constexpr u32 wait[4] = {4, 3, 2, 8};
        return (1u + wait[waitcnt_ & 3u]) * width;
    }
    return 1;
}
