#pragma once

#include "../types.hpp"
#include <atomic>

class Bus;

// Converts the GBA's 32.768 kHz stereo stream to 48 kHz host samples.
// The host audio thread is the sole consumer of the Bus audio queue.
class AudioOutput {
public:
    explicit AudioOutput(Bus& bus) : bus_(bus) {}
    bool Ready() const;
    void Fill(s16* interleaved_stereo, unsigned frames);
    void SetPlaybackSpeedPercent(unsigned percent) { speed_percent_.store(percent, std::memory_order_relaxed); }
    u64 Underruns() const { return underruns_.load(std::memory_order_relaxed); }
    u64 OutputFrames() const { return output_frames_.load(std::memory_order_relaxed); }

private:
    void PullNext();

    Bus& bus_;
    s16 previous_left_ = 0, previous_right_ = 0;
    s16 current_left_ = 0, current_right_ = 0;
    u64 phase_ = 0;
    s64 dc_left_ = 0, dc_right_ = 0;
    bool initialized_ = false;
    std::atomic<unsigned> speed_percent_{100};
    unsigned active_speed_percent_ = 100;
    unsigned transition_frames_ = 0;
    std::atomic<u64> underruns_{0}, output_frames_{0};
};
