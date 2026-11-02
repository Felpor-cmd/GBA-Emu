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
    const unsigned speed = std::clamp(speed_percent_.load(std::memory_order_relaxed), 100u, 1000u);
    const bool returning_to_normal = active_speed_percent_ > 100 && speed == 100;
    if (speed != active_speed_percent_) {
        if ((speed == 100) != (active_speed_percent_ == 100)) transition_frames_ = 240;
        active_speed_percent_ = speed;
    }
    const u32 queued = bus_.QueuedAudioSamples();
    // Keep at least one device callback's worth of source samples at high
    // speeds. A fixed 4096-sample trim starves a 1024-frame callback at 10x.
    const u32 callback_source = static_cast<u32>((1024ull * Sound::kSampleRate * speed +
        kOutputRate * 100u - 1u) / (kOutputRate * 100u));
    const u32 keep = returning_to_normal ? 2048u : 2048u + callback_source;
    if ((returning_to_normal && queued > keep) || (speed > 100 && queued > keep + 6144u)) {
        s16 discarded_left = 0, discarded_right = 0;
        while (bus_.QueuedAudioSamples() > keep)
            bus_.PopAudioSample(discarded_left, discarded_right);
        previous_left_ = current_left_ = discarded_left;
        previous_right_ = current_right_ = discarded_right;
        phase_ = 0;
        transition_frames_ = 240;
    }
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
        constexpr u64 period = static_cast<u64>(kOutputRate) * 100;
        s32 left = previous_left_ + static_cast<s32>((static_cast<s64>(left_delta) *
            static_cast<s64>(phase_)) / static_cast<s64>(period));
        s32 right = previous_right_ + static_cast<s32>((static_cast<s64>(right_delta) *
            static_cast<s64>(phase_)) / static_cast<s64>(period));
        if (transition_frames_) {
            const u32 gain = 240 - transition_frames_--;
            left = left * static_cast<s32>(gain) / 240;
            right = right * static_cast<s32>(gain) / 240;
        }
        // AC coupling, matching the 1/512 DC decay in mGBA 0.10.5's blip
        // output. Apply to the whole stereo mix, without changing its pitch.
        const auto couple = [](s32 input, s64& dc) {
            const s64 high = static_cast<s64>(input) * 65536 - dc;
            dc += high / 512;
            // The synthesized mix reaches the output at two-thirds of the
            // pinned mGBA frontend level. Match its playback gain so the
            // quiet decay at the end of the BIOS jingle remains audible.
            return static_cast<s16>(std::clamp<s64>((high / 65536) * 3 / 2, -32768, 32767));
        };
        samples[frame * 2] = couple(left, dc_left_);
        samples[frame * 2 + 1] = couple(right, dc_right_);
        phase_ += static_cast<u64>(Sound::kSampleRate) * speed;
        while (phase_ >= period) {
            phase_ -= period;
            PullNext();
        }
    }
}
