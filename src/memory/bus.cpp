#include "bus.hpp"

Bus::Bus(std::vector<u8> rom) : rom_(std::move(rom)) {}

u8 Bus::Read8(u32 address) const {
    // TODO: switch on the top bits of `address` to route to BIOS, EWRAM,
    // IWRAM, I/O registers, palette, VRAM, OAM, ROM, or SRAM.
    // Address ranges: https://problemkaputt.de/gbatek.htm#gbamemorymap

    switch (address >> 24){
        case 0x00: return bios_[address & (kBiosSize - 1)];
        
        case 0x02: return ewram_[address & (kEwramSize - 1)];
        case 0x03: return iwram_[address & (kIwramSize - 1)];
        case 0x05: return palette_[address & (kPaletteSize - 1)];
        case 0x06: return vram_[address & (kVramSize - 1)];
        case 0x07: return oam_[address & (kOamSize - 1)];
        
        default: return 0; // TODO: handle other regions (BIOS, ROM, SRAM, I/O registers)
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
    // TODO: same region routing as Read8, plus respect read-only regions
    // (BIOS, ROM) which should ignore writes.

    switch (address >> 24){
        case 0x00: break; // BIOS is read-only, ignore writes

        case 0x02: ewram_[address & (kEwramSize - 1)] = value; break;
        case 0x03: iwram_[address & (kIwramSize - 1)] = value; break;
        case 0x05: palette_[address & (kPaletteSize - 1)] = value; break;
        case 0x06: vram_[address & (kVramSize - 1)] = value; break;
        case 0x07: oam_[address & (kOamSize - 1)] = value; break;

        default: break; // TODO: handle other regions (BIOS, ROM, SRAM, I/O registers)
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
