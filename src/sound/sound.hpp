#pragma once

#include <array>
#include <atomic>
#include "../types.hpp"

// GBA audio unit: four PSG channels, two timer-clocked direct-sound FIFOs,
// and a lock-free single-producer/single-consumer queue for host audio.
class Sound {
public:
    static constexpr u32 kSampleRate = 32'768;

    Sound();
    u8 ReadRegister(u32 offset) const;
    void WriteRegister(u32 offset, u8 value);
    void Advance(u32 cycles);
    // Returns bit 0 for FIFO A and bit 1 for FIFO B when DMA should refill it.
    u8 TimerOverflow(unsigned timer);
    bool PopSample(s16& left, s16& right);
    u32 QueuedSamples() const;
    u64 DroppedSamples() const { return dropped_samples_.load(std::memory_order_relaxed); }
    s8 FifoSample(unsigned channel) const { return fifos_[channel].current; }
    u8 FifoSize(unsigned channel) const { return fifos_[channel].size; }

private:
    struct SquareChannel {
        bool active = false;
        u16 length = 0;
        u16 frequency = 0;
        u8 phase = 0;
        u8 volume = 0;
        u32 phase_cycles = 0;
        u32 envelope_cycles = 0;
        bool length_enabled = false;
    };
    struct WaveChannel {
        bool active = false;
        u16 length = 0;
        u16 frequency = 0;
        u8 position = 0;
        u32 phase_cycles = 0;
        bool length_enabled = false;
    };
    struct NoiseChannel {
        bool active = false;
        u16 length = 0;
        u16 lfsr = 0x4000;
        bool output_high = false;
        u8 volume = 0;
        u32 phase_cycles = 0;
        u32 envelope_cycles = 0;
        bool length_enabled = false;
    };
    struct Fifo {
        std::array<s8, 32> data{};
        u8 head = 0;
        u8 size = 0;
        s8 current = 0;
    };
    struct StereoSample { s16 left = 0, right = 0; };

    static constexpr u32 kRegisterBase = 0x60;
    static constexpr u32 kRegisterEnd = 0x8A;
    static constexpr u32 kRingCapacity = 16'384;
    static constexpr u32 kRingMask = kRingCapacity - 1;

    std::array<u8, kRegisterEnd - kRegisterBase> registers_{};
    std::array<u8, 32> wave_ram_{};
    SquareChannel square1_{};
    SquareChannel square2_{};
    WaveChannel wave_{};
    NoiseChannel noise_{};
    std::array<Fifo, 2> fifos_{};
    std::array<StereoSample, kRingCapacity> samples_{};
    std::atomic<u32> sample_write_{0};
    std::atomic<u32> sample_read_{0};
    std::atomic<u64> dropped_samples_{0};
    u32 sample_phase_ = 0;
    u32 pwm_phase_ = 0, pwm_samples_ = 0;
    s32 pwm_left_ = 0, pwm_right_ = 0;
    u32 sequencer_phase_ = 0;
    u8 sequencer_step_ = 0;
    u16 sweep_shadow_ = 0;
    u32 sweep_cycles_ = 0;
    bool sweep_enabled_ = false;

    u16 Register16(u32 offset) const;
    void WriteFifoByte(unsigned channel, u8 value);
    void ClearFifo(unsigned channel);
    void ClockSequencer();
    void ClockLength();
    void ClockEnvelope();
    void ClockSweep();
    void AdvanceOscillators(u32 cycles);
    void RestartSquare(SquareChannel& channel, u32 base, bool sweep);
    void RestartWave();
    void RestartNoise();
    void PushSample();
    void MixSample(s16& left, s16& right) const;
    int SquareOutput(const SquareChannel& channel, u32 base) const;
    int WaveOutput() const;
    int NoiseOutput() const;
    void PushHostSample(s16 left, s16 right);
};
