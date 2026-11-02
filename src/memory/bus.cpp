#include "bus.hpp"

#include <algorithm>
#include <stdexcept>

namespace {
u32 VramOffset(u32 address) {
    const u32 offset = address & 0x1FFFFu;
    return offset >= 0x18000u ? offset - 0x8000u : offset;
}
}

Bus::Bus(std::vector<u8> rom, std::vector<u8> bios) : backup_(rom), rom_(std::move(rom)) {
    if (!bios.empty() && bios.size() != kBiosSize)
        throw std::invalid_argument("BIOS must be exactly 16384 bytes");
    has_bios_ = !bios.empty();
    io_[0x300] = has_bios_ ? 0 : 1;
    io_[0x130] = 0xFF;
    io_[0x131] = 0x03;
    std::copy_n(bios.begin(), std::min(bios.size(), bios_.size()), bios_.begin());
}

void Bus::LatchBiosPrefetch(u32 address, bool thumb) {
    if (!has_bios_ || address >= kBiosSize) return;
    const u32 offset = address + (thumb ? 4u : 8u);
    if (offset + 3 < kBiosSize) {
        bios_prefetch_ = static_cast<u32>(bios_[offset]) |
            (static_cast<u32>(bios_[offset + 1]) << 8) |
            (static_cast<u32>(bios_[offset + 2]) << 16) |
            (static_cast<u32>(bios_[offset + 3]) << 24);
    }
}

u8 Bus::Read8(u32 address) const {
    switch (address >> 24){
        case 0x00:
            if (address >= kBiosSize) return 0;
            if (has_bios_ && !cpu_in_bios_)
                return static_cast<u8>(bios_prefetch_ >> ((address & 3u) * 8));
            return bios_[address];

        case 0x04:
            if (address == 0x04000300u) return io_[0x300] & 1u;
            if (address == 0x04000301u) return 0;
            if (address >= 0x04000060u && address < 0x040000A8u)
                return sound_.ReadRegister(address & 0x3FFu);
            if (address >= 0x04000100u && address < 0x04000110u) {
                const u32 timer_offset = address - 0x04000100u;
                const auto& timer = timers_[timer_offset / 4];
                switch (timer_offset & 3u) {
                    case 0: return static_cast<u8>(timer.counter);
                    case 1: return static_cast<u8>(timer.counter >> 8);
                    case 2: return timer.control;
                    default: return 0;
                }
            }
            if (address == 0x04000204) return static_cast<u8>(waitcnt_);
            if (address == 0x04000205) return static_cast<u8>(waitcnt_ >> 8);
            return io_[address & (kIoSize -1)];

        case 0x02: return ewram_[address & (kEwramSize - 1)];
        case 0x03: return iwram_[address & (kIwramSize - 1)];
        case 0x05: return palette_[address & (kPaletteSize - 1)];
        case 0x06: return vram_[VramOffset(address)];
        case 0x07: return oam_[address & (kOamSize - 1)];

        case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: {
            u32 offset = address & 0x01FFFFFF;    
            if (offset >= rom_.size())
                return 0; // Out of bounds
            else
                return rom_[offset]; // ROM is read-only, return value from ROM
        }

        case 0x0E: return backup_.ReadByte(address, hardware_cycles_);

        default: return 0;
    }
}

u16 Bus::Read16(u32 address) const {
    if (backup_.IsEeprom() && backup_.EepromMapped(address))
        return backup_.ReadEepromBit(hardware_cycles_);
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
            if (address == 0x04000300u) {
                if (has_bios_ && !cpu_in_bios_) break;
                io_[0x300] |= value & 1u;
                break;
            }
            if (address == 0x04000301u) {
                if (has_bios_ && !cpu_in_bios_) break;
                halt_requested_ = (value & 0x80u) == 0;
                break;
            }
            if (address >= 0x04000060u && address < 0x040000A8u) {
                TraceAudio(AudioEventKind::RegisterWrite, address, value);
                sound_.WriteRegister(address & 0x3FFu, value);
                break;
            }
            if (address >= 0x04000100u && address < 0x04000110u) {
                const u32 timer_offset = address - 0x04000100u;
                const unsigned timer_index = timer_offset / 4;
                auto& timer = timers_[timer_index];
                switch (timer_offset & 3u) {
                    case 0:
                        timer.reload = static_cast<u16>((timer.reload & 0xFF00u) | value);
                        break;
                    case 1:
                        timer.reload = static_cast<u16>((timer.reload & 0x00FFu) |
                                                        (static_cast<u16>(value) << 8));
                        break;
                    case 2: {
                        const u8 allowed_bits = timer_index == 0 ? 0xC3u : 0xC7u;
                        const u8 new_control = value & allowed_bits;
                        const bool was_enabled = (timer.control & 0x80u) != 0;
                        const bool enabled = (new_control & 0x80u) != 0;
                        if (!was_enabled && enabled) timer.counter = timer.reload;
                        if (was_enabled != enabled || ((timer.control ^ new_control) & 0x07u))
                            timer.prescaler_phase = 0;
                        timer.control = new_control;
                        break;
                    }
                    default: break;
                }
                break;
            }
            if (address == 0x04000202u || address == 0x04000203u) {
                io_[address & 0x3FFu] &= static_cast<u8>(~value);
                break;
            }
            if (address == 0x04000006u || address == 0x04000007u ||
                address == 0x04000130u || address == 0x04000131u) break;
            if (address == 0x04000004u) value = (value & 0x38u) | (io_[4] & 7u);
            if (address == 0x04000204) {
                waitcnt_ = (waitcnt_ & 0xFF00u) | value;
            } else if (address == 0x04000205) {
                waitcnt_ = (waitcnt_ & 0x00FFu) | (static_cast<u16>(value & 0x7Fu) << 8);
            } else {
                io_[address & (kIoSize - 1)] = value;
                for (unsigned channel = 0; channel < 4; ++channel) {
                    const u32 base = 0x040000B0u + channel * 12;
                    if (address != base + 11) continue;
                    auto& dma = dma_[channel];
                    const bool enabled = (value & 0x80u) != 0;
                    if (enabled && !dma.enabled) {
                        dma.source = Read32(base) & (channel == 0 ? 0x07FFFFFFu : 0x0FFFFFFFu);
                        dma.destination = Read32(base + 4) & (channel == 3 ? 0x0FFFFFFFu : 0x07FFFFFFu);
                        dma.reload = dma.destination;
                    }
                    dma.enabled = enabled;
                }
            }
            break;

        case 0x02: ewram_[address & (kEwramSize - 1)] = value; break;
        case 0x03: iwram_[address & (kIwramSize - 1)] = value; break;
        case 0x05: palette_[address & (kPaletteSize - 1)] = value; break;
        case 0x06: vram_[VramOffset(address)] = value; break;
        case 0x07: oam_[address & (kOamSize - 1)] = value; break;
        
        case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: {
            // ROM is read-only, ignore writes
            break;
        }
        
        case 0x0E: backup_.WriteByte(address, value, hardware_cycles_); break;

        default: break;
    }
}

void Bus::RequestInterrupt(u16 bits) {
    io_[0x202] |= static_cast<u8>(bits);
    io_[0x203] |= static_cast<u8>(bits >> 8);
}

u32 Bus::RunDma(unsigned timing) {
    // Timing mode 3 is sound FIFO (DMA1/2) or video capture (DMA3), not a
    // normal count-based transfer. Sound requests are dispatched by timers.
    if (timing == 3) return 0;
    u32 cycles = 0;
    for (unsigned channel = 0; channel < 4; ++channel) {
        auto& dma = dma_[channel];
        const u32 base = 0x040000B0u + channel * 12;
        const u16 control = Read16(base + 10);
        if (!dma.enabled || ((control >> 12) & 3u) != timing) continue;
        const u32 width = (control & (1u << 10)) ? 4 : 2;
        u32 count = Read16(base + 8) & (channel == 3 ? 0xFFFFu : 0x3FFFu);
        if (!count) count = channel == 3 ? 0x10000u : 0x4000u;
        dma.source &= ~(width - 1u);
        dma.destination &= ~(width - 1u);
        const unsigned src_mode = (control >> 7) & 3u, dst_mode = (control >> 5) & 3u;
        cycles += 2;
        for (u32 i = 0; i < count; ++i) {
            cycles += AccessCycles(dma.source, width, i != 0) + AccessCycles(dma.destination, width, i != 0);
            if (width == 4) Write32(dma.destination, Read32(dma.source));
            else {
                const u16 value = Read16(dma.source);
                if (channel == 3 && dma.destination >> 24 == 0x0D &&
                    backup_.EepromMapped(dma.destination) &&
                    (backup_.IsEeprom() || backup_.Type() == BackupType::Auto))
                    backup_.WriteEepromBit(value, count - i, hardware_cycles_ + cycles);
                else Write16(dma.destination, value);
            }
            if (src_mode == 0) dma.source += width;
            else if (src_mode == 1) dma.source -= width;
            if (dst_mode == 0 || dst_mode == 3) dma.destination += width;
            else if (dst_mode == 1) dma.destination -= width;
        }
        if (control & (1u << 14)) RequestInterrupt(static_cast<u16>(1u << (8 + channel)));
        if (timing && (control & (1u << 9))) {
            if (dst_mode == 3) dma.destination = dma.reload;
        } else {
            dma.enabled = false;
            io_[(base + 11) & 0x3FFu] &= 0x7Fu;
        }
    }
    return cycles;
}

u64 Bus::IncrementTimer(unsigned timer_index, u64 ticks) {
    if (!ticks) return 0;
    auto& timer = timers_[timer_index];
    const u64 clocks_to_overflow = 0x10000u - timer.counter;
    if (ticks < clocks_to_overflow) {
        timer.counter = static_cast<u16>(timer.counter + ticks);
        return 0;
    }

    const u64 ticks_after_first_overflow = ticks - clocks_to_overflow;
    const u64 period = 0x10000u - timer.reload;
    const u64 overflows = 1 + ticks_after_first_overflow / period;
    timer.counter = static_cast<u16>(timer.reload + ticks_after_first_overflow % period);
    return overflows;
}

u32 Bus::CyclesToTimerEvent() const {
    constexpr u32 prescalers[] = {1, 64, 256, 1024};
    u32 next = 0xFFFFFFFFu;
    for (unsigned index = 0; index < timers_.size(); ++index) {
        const auto& timer = timers_[index];
        if (!(timer.control & 0x80u) || (index && (timer.control & 4u))) continue;
        const u32 until = (0x10000u - timer.counter) * prescalers[timer.control & 3u] - timer.prescaler_phase;
        next = std::min(next, until);
    }
    return next;
}

u32 Bus::AdvanceTimers(u32 cycles) {
    constexpr u32 prescalers[] = {1, 64, 256, 1024};
    u32 dma_cycles = 0;
    while (cycles) {
        const u32 event = CyclesToTimerEvent();
        const u32 elapsed = std::min(cycles, event);
        // Reach the event chronologically. At a shared timer/DAC boundary,
        // the newly clocked FIFO byte is sampled by the DAC.
        const bool event_boundary = elapsed == event;
        if (event_boundary && elapsed > 1) sound_.Advance(elapsed - 1);
        hardware_cycles_ += elapsed;
        std::array<u64, 4> overflows{};
        for (unsigned index = 0; index < timers_.size(); ++index) {
            auto& timer = timers_[index];
            if (!(timer.control & 0x80u)) continue;
            u64 ticks = overflows[index ? index - 1 : 0];
            if (!index || !(timer.control & 4u)) {
                const u32 divisor = prescalers[timer.control & 3u];
                const u64 total = static_cast<u64>(timer.prescaler_phase) + elapsed;
                ticks = total / divisor;
                timer.prescaler_phase = static_cast<u16>(total % divisor);
            }
            overflows[index] = IncrementTimer(index, ticks);
            if (overflows[index]) TraceAudio(AudioEventKind::TimerOverflow, index, timer.counter);
            if (overflows[index] && (timer.control & 0x40u))
                RequestInterrupt(static_cast<u16>(1u << (3 + index)));
            if (index < 2 && overflows[index]) {
                const u8 requests = sound_.TimerOverflow(index);
                if (audio_trace_ && (Read16(0x04000084) & 0x80u)) {
                    const u16 control = Read16(0x04000082);
                    for (unsigned fifo = 0; fifo < 2; ++fifo)
                        if (((control >> (fifo ? 14 : 10)) & 1u) == index)
                            TraceAudio(AudioEventKind::FifoSample, fifo,
                                static_cast<u8>(sound_.FifoSample(fifo)) | (static_cast<u32>(sound_.FifoSize(fifo)) << 8));
                }
                dma_cycles += RunSoundDma(requests);
            }
        }
        sound_.Advance(event_boundary ? 1 : elapsed);
        cycles -= elapsed;
    }
    return dma_cycles;
}

u32 Bus::RunSoundDma(u8 requests) {
    u32 cycles = 0;
    for (unsigned fifo = 0; fifo < 2; ++fifo) {
        if (!(requests & (1u << fifo))) continue;
        const u32 fifo_address = fifo == 0 ? 0x040000A0u : 0x040000A4u;
        for (unsigned channel = 1; channel <= 2; ++channel) {
            auto& dma = dma_[channel];
            const u32 base = 0x040000B0u + channel * 12;
            const u16 control = Read16(base + 10);
            if (!dma.enabled || ((control >> 12) & 3u) != 3u ||
                !(control & (1u << 9)) || Read32(base + 4) != fifo_address ||
                ((control >> 7) & 3u) == 3u) continue;

            dma.source &= ~3u;
            TraceAudio(AudioEventKind::DmaRefill, channel, dma.source);
            cycles += 2; // DMA setup.
            for (unsigned word = 0; word < 4; ++word) {
                cycles += AccessCycles(dma.source, 4, word != 0) +
                          AccessCycles(fifo_address, 4, word != 0);
                Write32(fifo_address, Read32(dma.source));
                const unsigned source_mode = (control >> 7) & 3u;
                if (source_mode == 0) dma.source += 4;
                else if (source_mode == 1) dma.source -= 4;
            }
            if (control & (1u << 14)) RequestInterrupt(static_cast<u16>(1u << (8 + channel)));
            break; // DMA1 has priority over DMA2 for the same FIFO request.
        }
    }
    return cycles;
}

bool Bus::PopAudioSample(s16& left, s16& right) {
    return sound_.PopSample(left, right);
}

void Bus::Write16(u32 address, u16 value) {
    if (backup_.IsEeprom() && backup_.EepromMapped(address)) {
        backup_.WriteEepromBit(value, 1, hardware_cycles_);
        return;
    }
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

void Bus::SetKeyInput(u16 pressed) {
    const u16 state = static_cast<u16>(0x03FFu & ~pressed);
    io_[0x130] = static_cast<u8>(state);
    io_[0x131] = static_cast<u8>(state >> 8);
}

void Bus::ResetMemory(u32 flags) {
    if (flags & (1u << 0)) ewram_.fill(0);
    if (flags & (1u << 1)) std::fill(iwram_.begin(), iwram_.end() - 0x200, 0);
    if (flags & (1u << 2)) palette_.fill(0);
    if (flags & (1u << 3)) vram_.fill(0);
    if (flags & (1u << 4)) oam_.fill(0);
    // RegisterRamReset's documented SIODATA32 side effect occurs for every mask.
    Write16(0x04000120u, 0);
    if (flags & (1u << 5)) {
        for (u32 offset : {0x128u, 0x12Au, 0x140u, 0x150u, 0x152u, 0x154u, 0x156u})
            Write16(0x04000000u + offset, 0);
        Write16(0x04000134u, 0x8000);
    }
    if (flags & (1u << 6)) {
        Write16(0x04000084u, 0);
        Write16(0x04000082u, 0x8800u); // Reset both direct-sound FIFOs.
        Write16(0x04000088u, 0x0200);
    }
    if (flags & (1u << 7)) {
        for (u32 offset = 4; offset <= 0x54; offset += 2) Write16(0x04000000u + offset, 0);
        for (u32 offset : {0x20u, 0x26u, 0x30u, 0x36u}) Write16(0x04000000u + offset, 0x100);
        for (u32 offset = 0xB0; offset < 0xE0; offset += 2) Write16(0x04000000u + offset, 0);
        for (u32 offset = 0x100; offset < 0x110; offset += 2) Write16(0x04000000u + offset, 0);
        timers_.fill(Timer{});
        Write16(0x04000200u, 0);
        Write16(0x04000202u, 0xFFFF);
        Write16(0x04000204u, 0);
        Write16(0x04000208u, 0);
    }
    Write16(0x04000000u, 0x0080u);
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
