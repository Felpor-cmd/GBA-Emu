#include "sound.hpp"

#include <algorithm>
#include <limits>

namespace {
constexpr u32 kCpuClock = 16'777'216;
constexpr u32 kSampleCycles = kCpuClock / Sound::kSampleRate;
constexpr u32 kSequencerCycles = kCpuClock / 512;

u8 ReadMask(u32 offset) {
    switch (offset) {
        case 0x60: return 0x7F;
        case 0x62: case 0x68: return 0xC0;
        case 0x64: case 0x6C: case 0x72: case 0x74: case 0x78: return 0;
        case 0x61: case 0x66: case 0x67: case 0x6A: case 0x6B:
        case 0x71: case 0x76: case 0x77: case 0x7A: case 0x7B:
        case 0x86: case 0x87: return 0;
        case 0x65: case 0x6D: case 0x75: return 0x40;
        case 0x70: return 0xE0;
        case 0x73: return 0xE0;
        case 0x7D: return 0x40;
        case 0x80: return 0x77;
        case 0x82: return 0x0F;
        case 0x83: return 0x77;
        case 0x84: return 0x8F;
        case 0x85: return 0;
        case 0x88: return 0xFE;
        case 0x89: return 0xC3;
        default: return 0xFF;
    }
}

u8 WriteMask(u32 offset) {
    switch (offset) {
        case 0x60: return 0x7F;
        case 0x61: case 0x66: case 0x67: case 0x6A: case 0x6B:
        case 0x71: case 0x76: case 0x77: case 0x7A: case 0x7B:
        case 0x85: case 0x86: case 0x87: return 0;
        case 0x65: case 0x6D: case 0x75: return 0x47;
        case 0x70: return 0xE0;
        case 0x73: return 0xE0;
        case 0x78: return 0x3F;
        case 0x7D: return 0x40;
        case 0x80: return 0x77;
        case 0x82: return 0x0F;
        case 0x83: return 0x77;
        case 0x84: return 0x80;
        case 0x88: return 0xFE;
        case 0x89: return 0xC3;
        default: return 0xFF;
    }
}

s16 ClampS16(s32 value) {
    return static_cast<s16>(std::clamp(value,
        static_cast<s32>(std::numeric_limits<s16>::min()),
        static_cast<s32>(std::numeric_limits<s16>::max())));
}
}

Sound::Sound() {
    registers_[0x89 - kRegisterBase] = 0x02; // SOUNDBIAS power-on/reset value: 0x0200.
}

u16 Sound::Register16(u32 offset) const {
    const std::size_t index = offset - kRegisterBase;
    return static_cast<u16>(registers_[index] | (static_cast<u16>(registers_[index + 1]) << 8));
}

u8 Sound::ReadRegister(u32 offset) const {
    if (offset >= 0x90 && offset < 0xA0) {
        const unsigned index = offset - 0x90;
        const unsigned selected = (Register16(0x70) >> 6) & 1u;
        const unsigned bank = selected ^ (wave_.active ? 1u : 0u);
        return wave_ram_[bank * 16 + index];
    }
    if (offset >= 0xA0 && offset < 0xA8) return 0;
    if (offset < kRegisterBase || offset >= kRegisterEnd) return 0;
    if (offset == 0x84)
        return static_cast<u8>((Register16(0x84) & 0x80u) |
            (square1_.active ? 1u : 0u) | (square2_.active ? 2u : 0u) |
            (wave_.active ? 4u : 0u) | (noise_.active ? 8u : 0u));
    return registers_[offset - kRegisterBase] & ReadMask(offset);
}

void Sound::WriteFifoByte(unsigned channel, u8 value) {
    auto& fifo = fifos_[channel];
    if (fifo.size == fifo.data.size()) return;
    const u8 tail = static_cast<u8>((fifo.head + fifo.size) & 31u);
    fifo.data[tail] = static_cast<s8>(value);
    ++fifo.size;
}

void Sound::ClearFifo(unsigned channel) {
    fifos_[channel] = Fifo{};
}

void Sound::WriteRegister(u32 offset, u8 value) {
    if (offset >= 0xA0 && offset < 0xA4) { WriteFifoByte(0, value); return; }
    if (offset >= 0xA4 && offset < 0xA8) { WriteFifoByte(1, value); return; }
    if (offset >= 0x90 && offset < 0xA0) {
        const unsigned index = offset - 0x90;
        const unsigned selected = (Register16(0x70) >> 6) & 1u;
        const unsigned bank = selected ^ (wave_.active ? 1u : 0u);
        wave_ram_[bank * 16 + index] = value;
        return;
    }
    if (offset < kRegisterBase || offset >= kRegisterEnd) return;

    if (offset == 0x84) {
        const bool was_enabled = (Register16(0x84) & 0x80u) != 0;
        registers_[offset - kRegisterBase] = value & 0x80u;
        const bool enabled = (value & 0x80u) != 0;
        if (was_enabled && !enabled) {
            std::fill(registers_.begin(), registers_.begin() + (0x82 - kRegisterBase), 0);
            square1_ = SquareChannel{};
            square2_ = SquareChannel{};
            wave_ = WaveChannel{};
            noise_ = NoiseChannel{};
            noise_filtered_ = 0;
            noise_integral_ = 0;
        }
        return;
    }
    if (offset >= 0x60 && offset <= 0x81 && !(Register16(0x84) & 0x80u)) return;

    const u8 masked_value = value & WriteMask(offset);
    registers_[offset - kRegisterBase] = masked_value;

    if (offset == 0x83) {
        if (value & 0x08u) ClearFifo(0);
        if (value & 0x80u) ClearFifo(1);
    } else if (offset == 0x65 && (value & 0x80u)) {
        RestartSquare(square1_, 0x62, true);
    } else if (offset == 0x6D && (value & 0x80u)) {
        RestartSquare(square2_, 0x68, false);
    } else if (offset == 0x75 && (value & 0x80u)) {
        RestartWave();
    } else if (offset == 0x7D && (value & 0x80u)) {
        RestartNoise();
    }
}

void Sound::RestartSquare(SquareChannel& channel, u32 base, bool sweep) {
    const u16 duty_length = Register16(base);
    const u16 frequency_control = Register16(base + 2);
    channel.active = true;
    channel.frequency = frequency_control & 0x07FFu;
    channel.length = static_cast<u16>(64 - (duty_length & 0x3Fu));
    if (!channel.length) channel.length = 64;
    channel.length_enabled = (frequency_control & 0x4000u) != 0;
    channel.phase = 0;
    channel.phase_cycles = 0;
    channel.envelope_cycles = 0;
    channel.volume = static_cast<u8>(Register16(base) >> 12);
    if (!(Register16(base) & 0xF800u)) channel.active = false;

    if (sweep) {
        sweep_shadow_ = channel.frequency;
        sweep_cycles_ = 0;
        const u16 control = Register16(0x60);
        sweep_enabled_ = ((control >> 4) & 7u) != 0 || (control & 7u) != 0;
        if ((control & 7u) && (sweep_shadow_ + (sweep_shadow_ >> (control & 7u))) > 2047)
            channel.active = false;
    }
}

void Sound::RestartWave() {
    const u16 control = Register16(0x74);
    wave_.active = (Register16(0x70) & 0x0080u) != 0;
    wave_.frequency = control & 0x07FFu;
    wave_.length = static_cast<u16>(256 - (Register16(0x72) & 0x00FFu));
    if (!wave_.length) wave_.length = 256;
    wave_.length_enabled = (control & 0x4000u) != 0;
    wave_.position = 0;
    wave_.phase_cycles = 0;
}

void Sound::RestartNoise() {
    const u16 control = Register16(0x7C);
    noise_.active = true;
    noise_.length = static_cast<u16>(64 - (Register16(0x78) & 0x003Fu));
    if (!noise_.length) noise_.length = 64;
    noise_.length_enabled = (control & 0x4000u) != 0;
    noise_.volume = static_cast<u8>(Register16(0x78) >> 12);
    noise_.envelope_cycles = 0;
    noise_.phase_cycles = 0;
    noise_.lfsr = (Register16(0x7C) & 0x0008u) ? 0x40u : 0x4000u;
    noise_.output_high = false;
    noise_filtered_ = 0;
    if (!(Register16(0x78) & 0xF800u)) noise_.active = false;
}

u8 Sound::TimerOverflow(unsigned timer) {
    if (!(Register16(0x84) & 0x80u)) return 0;
    u8 requests = 0;
    const u16 control = Register16(0x82);
    for (unsigned channel = 0; channel < fifos_.size(); ++channel) {
        const unsigned timer_select_bit = channel == 0 ? 10u : 14u;
        if (((control >> timer_select_bit) & 1u) != timer) continue;
        auto& fifo = fifos_[channel];
        if (fifo.size) {
            fifo.current = fifo.data[fifo.head];
            fifo.head = static_cast<u8>((fifo.head + 1u) & 31u);
            --fifo.size;
        } else {
            fifo.current = 0;
        }
        if (fifo.size <= 16) requests |= static_cast<u8>(1u << channel);
    }
    return requests;
}

void Sound::AdvanceOscillators(u32 cycles) {
    const auto advance_square = [cycles](SquareChannel& channel) {
        if (!channel.active) return;
        const u32 period = 16u * (2048u - channel.frequency);
        const u64 total = static_cast<u64>(channel.phase_cycles) + cycles;
        channel.phase = static_cast<u8>((channel.phase + total / period) & 7u);
        channel.phase_cycles = static_cast<u32>(total % period);
    };
    advance_square(square1_);
    advance_square(square2_);

    if (wave_.active) {
        const u32 period = 8u * (2048u - wave_.frequency);
        const u64 total = static_cast<u64>(wave_.phase_cycles) + cycles;
        const u32 length = (Register16(0x70) & 0x20u) ? 64u : 32u;
        wave_.position = static_cast<u8>((wave_.position + total / period) % length);
        wave_.phase_cycles = static_cast<u32>(total % period);
    }

    if (noise_.active) {
        const u16 polynomial = Register16(0x7C);
        const u32 ratio = polynomial & 7u;
        // GBATEK: 524288 / r / 2^(s+1) Hz, with r=0 interpreted as 0.5.
        const u32 base = (ratio ? ratio * 64u : 32u) << ((polynomial >> 4) & 15u);
        u32 remaining = cycles;
        while (remaining) {
            const u32 step = std::min(remaining, base - noise_.phase_cycles);
            noise_integral_ += NoiseOutput() * static_cast<s32>(step);
            noise_.phase_cycles += step;
            remaining -= step;
            if (noise_.phase_cycles == base) {
                noise_.phase_cycles = 0;
                noise_.output_high = (noise_.lfsr & 1u) != 0;
                noise_.lfsr >>= 1;
                if (noise_.output_high)
                    noise_.lfsr ^= (polynomial & 0x0008u) ? 0x60u : 0x6000u;
            }
        }
    }
}

void Sound::ClockLength() {
    const auto tick = [](auto& channel) {
        if (!channel.active || !channel.length_enabled || !channel.length) return;
        if (--channel.length == 0) channel.active = false;
    };
    tick(square1_);
    tick(square2_);
    tick(wave_);
    tick(noise_);
}

void Sound::ClockEnvelope() {
    const auto tick = [this](auto& channel, u32 envelope_register) {
        const u16 envelope = Register16(envelope_register);
        const u32 period = (envelope >> 8) & 7u;
        if (!channel.active || !period) return;
        channel.envelope_cycles += 1;
        if (channel.envelope_cycles < period) return;
        channel.envelope_cycles = 0;
        const bool increase = (envelope & 0x0800u) != 0;
        if (increase && channel.volume < 15) ++channel.volume;
        else if (!increase && channel.volume > 0) --channel.volume;
    };
    tick(square1_, 0x62);
    tick(square2_, 0x68);
    tick(noise_, 0x78);
}

void Sound::ClockSweep() {
    const u16 sweep = Register16(0x60);
    const u32 period = (sweep >> 4) & 7u;
    const u32 shift = sweep & 7u;
    if (!square1_.active || !sweep_enabled_ || !period) return;
    if (++sweep_cycles_ < period) return;
    sweep_cycles_ = 0;
    if (!shift) return;
    const u32 delta = sweep_shadow_ >> shift;
    u32 frequency = 0;
    if (sweep & 0x08u) {
        frequency = sweep_shadow_ - delta;
    } else {
        frequency = sweep_shadow_ + delta;
    }
    if (frequency > 2047) { square1_.active = false; return; }
    sweep_shadow_ = static_cast<u16>(frequency);
    square1_.frequency = static_cast<u16>(frequency);
    const u32 second = (sweep & 0x08u) ? frequency - (frequency >> shift)
                                       : frequency + (frequency >> shift);
    if (second > 2047) square1_.active = false;
}

void Sound::ClockSequencer() {
    if ((sequencer_step_ & 1u) == 0) ClockLength();
    if (sequencer_step_ == 2 || sequencer_step_ == 6) ClockSweep();
    if (sequencer_step_ == 7) ClockEnvelope();
    sequencer_step_ = static_cast<u8>((sequencer_step_ + 1u) & 7u);
}

int Sound::SquareOutput(const SquareChannel& channel, u32 base) const {
    if (!channel.active || !channel.volume) return 0;
    const unsigned duty = (Register16(base) >> 6) & 3u;
    constexpr u8 patterns[] = {0x01, 0x03, 0x0F, 0x7F};
    const int amplitude = static_cast<int>(channel.volume) * 8;
    return (patterns[duty] & (1u << channel.phase)) ? amplitude : -amplitude;
}

int Sound::WaveOutput() const {
    if (!wave_.active) return 0;
    const u16 control = Register16(0x70);
    const unsigned selected = (control >> 6) & 1u;
    const unsigned position = wave_.position;
    const unsigned bank = selected ^ ((position >= 32 && (control & 0x20u)) ? 1u : 0u);
    const unsigned digit = position & 31u;
    const u8 packed = wave_ram_[bank * 16 + digit / 2];
    const int sample = (digit & 1u) ? (packed & 0x0Fu) : (packed >> 4);
    const u16 volume_control = Register16(0x72);
    int volume_mode = (volume_control >> 13) & 3u;
    int amplitude = sample - 8;
    if (volume_control & 0x8000u) amplitude = amplitude * 3 / 4;
    else if (volume_mode == 0) return 0;
    else if (volume_mode == 2) amplitude /= 2;
    else if (volume_mode == 3) amplitude /= 4;
    return amplitude * 8;
}

int Sound::NoiseOutput() const {
    if (!noise_.active || !noise_.volume) return 0;
    const int amplitude = static_cast<int>(noise_.volume) * 8;
    return noise_.output_high ? amplitude : -amplitude;
}

void Sound::MixSample(s16& left, s16& right) {
    // A short low-pass stage keeps the fast PSG noise from folding into
    // harsh 32 kHz sample-to-sample edges.
    noise_filtered_ = (noise_filtered_ + noise_integral_ / static_cast<int>(kSampleCycles)) / 2;
    if (!(Register16(0x84) & 0x80u)) { left = right = 0; return; }
    const u16 pan = Register16(0x80);
    const u16 direct = Register16(0x82);
    const int psg_volume = static_cast<int>(direct & 3u);
    const int psg_scale = psg_volume == 0 ? 1 : psg_volume == 1 ? 2 : 4;
    s32 left_mix = 0, right_mix = 0;
    const int outputs[] = {SquareOutput(square1_, 0x62), SquareOutput(square2_, 0x68), WaveOutput(), noise_filtered_};
    for (unsigned channel = 0; channel < 4; ++channel) {
        const int value = outputs[channel] * psg_scale / 4;
        if (pan & (1u << (8 + channel))) right_mix += value;
        if (pan & (1u << (12 + channel))) left_mix += value;
    }
    left_mix = left_mix * static_cast<int>((pan >> 4) & 7u) / 7;
    right_mix = right_mix * static_cast<int>(pan & 7u) / 7;

    for (unsigned channel = 0; channel < 2; ++channel) {
        const unsigned right_bit = channel == 0 ? 8u : 12u;
        const unsigned left_bit = channel == 0 ? 9u : 13u;
        const unsigned volume_bit = channel == 0 ? 2u : 3u;
        const int value = static_cast<int>(fifos_[channel].current) * 4;
        const int scaled = (direct & (1u << volume_bit)) ? value : value / 2;
        if (direct & (1u << right_bit)) right_mix += scaled;
        if (direct & (1u << left_bit)) left_mix += scaled;
    }
    left = static_cast<s16>(ClampS16(left_mix * 32));
    right = static_cast<s16>(ClampS16(right_mix * 32));
}

void Sound::PushHostSample(s16 left, s16 right) {
    const u32 write = sample_write_.load(std::memory_order_relaxed);
    const u32 next = (write + 1u) & kRingMask;
    if (next == sample_read_.load(std::memory_order_acquire)) return;
    samples_[write] = {left, right};
    sample_write_.store(next, std::memory_order_release);
}

bool Sound::PopSample(s16& left, s16& right) {
    const u32 read = sample_read_.load(std::memory_order_relaxed);
    if (read == sample_write_.load(std::memory_order_acquire)) return false;
    const StereoSample sample = samples_[read];
    sample_read_.store((read + 1u) & kRingMask, std::memory_order_release);
    left = sample.left;
    right = sample.right;
    return true;
}

u32 Sound::QueuedSamples() const {
    const u32 write = sample_write_.load(std::memory_order_acquire);
    const u32 read = sample_read_.load(std::memory_order_acquire);
    return (write - read) & kRingMask;
}

void Sound::PushSample() {
    s16 left = 0, right = 0;
    MixSample(left, right);
    PushHostSample(left, right);
    noise_integral_ = 0;
}

void Sound::Advance(u32 cycles) {
    while (cycles) {
        const u32 sample_remaining = kSampleCycles - sample_phase_;
        const u32 amount = std::min(cycles, sample_remaining);
        AdvanceOscillators(amount);
        sample_phase_ += amount;
        sequencer_phase_ += amount;
        cycles -= amount;
        if (sequencer_phase_ == kSequencerCycles) {
            sequencer_phase_ = 0;
            ClockSequencer();
        }
        if (sample_phase_ == kSampleCycles) {
            sample_phase_ = 0;
            PushSample();
        }
    }
}
