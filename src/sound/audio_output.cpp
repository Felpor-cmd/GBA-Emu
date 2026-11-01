#include "audio_output.hpp"

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
    if (!bus_.PopAudioSample(current_left_, current_right_))
        current_left_ = current_right_ = 0;
}

void AudioOutput::Fill(s16* samples, unsigned frames) {
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
        samples[frame * 2] = static_cast<s16>(previous_left_ +
            static_cast<s32>((static_cast<s64>(left_delta) * phase_) / kOutputRate));
        samples[frame * 2 + 1] = static_cast<s16>(previous_right_ +
            static_cast<s32>((static_cast<s64>(right_delta) * phase_) / kOutputRate));
        phase_ += Sound::kSampleRate;
        while (phase_ >= kOutputRate) {
            phase_ -= kOutputRate;
            PullNext();
        }
    }
}
