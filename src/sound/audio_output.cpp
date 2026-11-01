#include "audio_output.hpp"

#include <algorithm>
#include "../memory/bus.hpp"

namespace {
constexpr u32 kOutputRate = 48'000;
constexpr s32 kTargetSamples = 2'048;
}

bool AudioOutput::Ready() const {
    return bus_.QueuedAudioSamples() >= static_cast<u32>(kTargetSamples);
}

void AudioOutput::PullNext() {
    previous_left_ = current_left_;
    previous_right_ = current_right_;
    if (!bus_.PopAudioSample(current_left_, current_right_)) {
        current_left_ = current_right_ = 0;
        underruns_.fetch_add(1, std::memory_order_relaxed);
    }
}

void AudioOutput::Fill(s16* samples, unsigned frames) {
    output_frames_.fetch_add(frames, std::memory_order_relaxed);
    if (!initialized_) {
        bus_.PopAudioSample(current_left_, current_right_);
        previous_left_ = current_left_;
        previous_right_ = current_right_;
        PullNext();
        initialized_ = true;
    }

    for (unsigned frame = 0; frame < frames; ++frame) {
        const s32 left_delta = static_cast<s32>(current_left_) - previous_left_;
        const s32 right_delta = static_cast<s32>(current_right_) - previous_right_;
        const s32 left = previous_left_ + static_cast<s32>((static_cast<s64>(left_delta) * phase_) / kOutputRate);
        const s32 right = previous_right_ + static_cast<s32>((static_cast<s64>(right_delta) * phase_) / kOutputRate);
        // AC coupling, matching the 1/512 DC decay in mGBA 0.10.5's blip
        // output. Apply to the whole stereo mix, without changing its pitch.
        const auto couple = [](s32 input, s64& dc) {
            const s64 high = static_cast<s64>(input) * 65536 - dc;
            dc += high / 512;
            return static_cast<s16>(std::clamp<s64>(high / 65536, -32768, 32767));
        };
        samples[frame * 2] = couple(left, dc_left_);
        samples[frame * 2 + 1] = couple(right, dc_right_);
        phase_ += Sound::kSampleRate;
        while (phase_ >= kOutputRate) {
            phase_ -= kOutputRate;
            PullNext();
        }
    }
}
