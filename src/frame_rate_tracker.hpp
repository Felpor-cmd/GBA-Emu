#pragma once

#include <algorithm>
#include <chrono>
#include <optional>

// Measures emulated time advanced per wall-clock time. The caller supplies a
// timestamp after each completed batch, so expensive frames remain in the
// denominator instead of making playback appear faster than the video.
class FrameRateTracker {
public:
    using Clock = std::chrono::steady_clock;

    explicit FrameRateTracker(double frame_period_seconds)
        : frame_period_seconds_(frame_period_seconds) {}

    void Reset(Clock::time_point now) {
        window_start_ = now;
        completed_frames_ = 0;
    }

    std::optional<unsigned> Observe(unsigned completed_frames, Clock::time_point now,
                                    unsigned maximum_percent) {
        completed_frames_ += completed_frames;
        const double elapsed = std::chrono::duration<double>(now - window_start_).count();
        if (elapsed < 0.05) return std::nullopt;
        const unsigned achieved = static_cast<unsigned>(
            100.0 * completed_frames_ * frame_period_seconds_ / elapsed);
        window_start_ = now;
        completed_frames_ = 0;
        return std::clamp(achieved, 50u, maximum_percent);
    }

private:
    double frame_period_seconds_;
    Clock::time_point window_start_{};
    unsigned completed_frames_ = 0;
};
