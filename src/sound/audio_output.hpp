#pragma once

#include "../types.hpp"

class Bus;

// Converts the GBA's 32.768 kHz stereo stream to 48 kHz host samples.
// The host audio thread is the sole consumer of the Bus audio queue.
class AudioOutput {
public:
    explicit AudioOutput(Bus& bus) : bus_(bus) {}
    bool Ready() const;
    void Fill(s16* interleaved_stereo, unsigned frames);

private:
    void PullNext();

    Bus& bus_;
    s16 previous_left_ = 0, previous_right_ = 0;
    s16 current_left_ = 0, current_right_ = 0;
    u32 phase_ = 0;
    bool initialized_ = false;
};
