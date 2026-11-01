#include <array>
#include <catch2/catch_test_macros.hpp>
#include "memory/bus.hpp"
#include "ppu/ppu.hpp"
#include "sound/audio_output.hpp"

TEST_CASE("Host audio playback rate does not depend on queue depth") {
    Bus bus({});
    AudioOutput output(bus);
    bus.AdvanceTimers(Ppu::kCyclesPerFrame * 4);
    REQUIRE(output.Ready());

    std::array<s16, 480 * 2> samples{};
    const u32 first_before = bus.QueuedAudioSamples();
    output.Fill(samples.data(), 480);
    const u32 first_consumed = first_before - bus.QueuedAudioSamples();

    bus.AdvanceTimers(Ppu::kCyclesPerFrame * 10);
    const u32 second_before = bus.QueuedAudioSamples();
    output.Fill(samples.data(), 480);
    const u32 second_consumed = second_before - bus.QueuedAudioSamples();

    // The first call primes two interpolation samples. Each later 10 ms
    // callback consumes 327 or 328 samples at the fixed GBA rate.
    REQUIRE(first_consumed >= 329);
    REQUIRE(first_consumed <= 330);
    REQUIRE(second_consumed >= 327);
    REQUIRE(second_consumed <= 328);
}
