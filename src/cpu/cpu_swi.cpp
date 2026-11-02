#include "cpu.hpp"

#include <stdexcept>
#include <string>
#include <cmath>
#include <sstream>
#include <array>

void Cpu::ExecuteSoftwareInterrupt(u32 instruction, u32 instruction_address) {
    const bool thumb = (cpsr_ & kThumbFlag) != 0;
    const u32 swi = thumb ? instruction & 0xFFu : (instruction >> 16) & 0xFFu;
    if (swi == 0x12) ++swi12_calls_;
    if (!emulate_bios_ || bus_.HasBios()) {
        EnterSoftwareInterruptException(instruction_address);
        return;
    }
    if (!bus_.HasBios() && swi == 1) { // RegisterRamReset
        bus_.ResetMemory(regs_[0]);
        regs_[15] = instruction_address + (thumb ? 2u : 4u);
        next_fetch_sequential_ = false;
        return;
    }
    if (emulate_bios_ && (swi == 0x0B || swi == 0x0C)) {
        const bool word = swi == 0x0C || (regs_[2] & (1u << 26));
        const bool fill = (regs_[2] & (1u << 24)) != 0;
        const u32 width = word ? 4u : 2u;
        u32 source = regs_[0] & ~(width - 1u), destination = regs_[1] & ~(width - 1u);
        u32 count = regs_[2] & 0x1FFFFFu;
        if (swi == 0x0C) count = (count + 7u) & ~7u;
        if (!count || source < 0x02000000u) return;
        const u32 fixed = fill ? (word ? ReadData32(source) : ReadData16(source)) : 0;
        for (u32 i = 0; i < count; ++i) {
            const u32 value = fill ? fixed : word ? ReadData32(source) : ReadData16(source);
            if (word) WriteData32(destination, value);
            else WriteData16(destination, static_cast<u16>(value));
            if (!fill) source += width;
            destination += width;
        }
        return;
    }
    if (swi == 0x11 || swi == 0x12) { // LZ77UnCompWram/Vram.
        u32 source = regs_[0] & ~3u;
        u32 destination = swi == 0x12 ? regs_[1] & ~1u : regs_[1];
        const u32 header = ReadData32(source);
        source += 4;
        if ((header & 0xFFu) != 0x10u)
            throw std::runtime_error("Invalid LZ77 header");
        const u32 size = header >> 8;
        std::array<u8, 4096> history{};
        u32 produced = 0;
        u8 low_byte = 0;
        const auto emit = [&](u8 value) {
            history[produced & 0xFFFu] = value;
            if (swi == 0x11) {
                WriteData8(destination++, value);
            } else if ((produced & 1u) == 0) low_byte = value;
            else {
                WriteData16(destination, static_cast<u16>(low_byte | (static_cast<u16>(value) << 8)));
                destination += 2;
            }
            ++produced;
        };
        while (produced < size) {
            const u8 flags = ReadData8(source++);
            for (unsigned bit = 0; bit < 8 && produced < size; ++bit) {
                if ((flags & (0x80u >> bit)) == 0) {
                    emit(ReadData8(source++));
                    continue;
                }
                const u8 first = ReadData8(source++);
                const u8 second = ReadData8(source++);
                const u32 length = (first >> 4) + 3u;
                const u32 distance = (((first & 15u) << 8) | second) + 1u;
                if (distance > produced)
                    throw std::runtime_error("Invalid LZ77 VRAM backreference");
                for (u32 i = 0; i < length && produced < size; ++i)
                    emit(history[(produced - distance) & 0xFFFu]);
            }
        }
        if (swi == 0x12 && (produced & 1u)) WriteData16(destination, low_byte);
        regs_[0] = source;
        regs_[1] = destination + (swi == 0x12 && (produced & 1u) ? 2u : 0u);
        regs_[3] = 0;
        return;
    }
    if (emulate_bios_ && (swi == 6 || swi == 7)) {
        const s64 numerator = static_cast<s32>(regs_[swi == 6 ? 0 : 1]);
        const s64 denominator = static_cast<s32>(regs_[swi == 6 ? 1 : 0]);
        if (!denominator) throw std::runtime_error("BIOS division by zero");
        const s64 quotient = numerator / denominator;
        regs_[0] = static_cast<u32>(quotient);
        regs_[1] = static_cast<u32>(numerator % denominator);
        regs_[3] = static_cast<u32>(quotient < 0 ? -quotient : quotient);
        return;
    }
    if (emulate_bios_ && swi == 8) { // Sqrt: floor(sqrt(unsigned r0)).
        const u32 input = regs_[0];
        u32 root = 0;
        for (u32 bit = 1u << 15; bit; bit >>= 1) {
            const u32 candidate = root | bit;
            if (candidate * candidate <= input) root = candidate;
        }
        regs_[0] = root;
        return;
    }
    if (emulate_bios_ && (swi == 0x0E || swi == 0x0F)) {
        u32 source = regs_[0], destination = regs_[1];
        for (u32 i = 0; i < regs_[2]; ++i) {
            const bool background = swi == 0x0E;
            const u32 scale_address = source + (background ? 12u : 0u);
            const s32 sx = static_cast<s16>(ReadData16(scale_address));
            const s32 sy = static_cast<s16>(ReadData16(scale_address + 2));
            const u32 angle = ReadData16(scale_address + 4) >> 8;
            constexpr double pi = 3.14159265358979323846;
            const s32 sine = static_cast<s32>(std::sin(angle * pi / 128.0) * 16384.0);
            const s32 cosine = static_cast<s32>(std::sin(((angle + 64) & 255u) * pi / 128.0) * 16384.0);
            const s32 pa = (sx * cosine) >> 14, pb = (-sx * sine) >> 14;
            const s32 pc = (sy * sine) >> 14, pd = (sy * cosine) >> 14;
            const u32 stride = background ? 2u : regs_[3];
            WriteData16(destination, static_cast<u16>(pa));
            WriteData16(destination + stride, static_cast<u16>(pb));
            WriteData16(destination + stride * 2, static_cast<u16>(pc));
            WriteData16(destination + stride * 3, static_cast<u16>(pd));
            if (background) {
                const s32 x = static_cast<s16>(ReadData16(source + 8));
                const s32 y = static_cast<s16>(ReadData16(source + 10));
                WriteData32(destination + 8, ReadData32(source) - pa * x - pb * y);
                WriteData32(destination + 12, ReadData32(source + 4) - pc * x - pd * y);
            }
            source += background ? 20 : 8;
            destination += background ? 16 : stride * 4;
        }
        return;
    }
    if (emulate_bios_ && (swi == 2 || swi == 4 || swi == 5)) {
        if (swi != 2) {
            bios_wait_mask_ = swi == 5 ? 1u : static_cast<u16>(regs_[1]);
            u16 flags = bus_.Read16(0x03007FF8u);
            if (swi == 5 || regs_[0]) flags &= ~bios_wait_mask_;
            bus_.Write16(0x03007FF8u, flags);
            bus_.Write16(0x04000208u, 1);
            if (flags & bios_wait_mask_) {
                bus_.Write16(0x03007FF8u, flags & ~bios_wait_mask_);
                bios_wait_mask_ = 0;
                return;
            }
        }
        halted_ = true;
        return;
    }
    std::ostringstream message;
    message << "Unsupported BIOS service 0x" << std::hex << swi << " at PC 0x" << instruction_address;
    throw std::runtime_error(message.str());
}
