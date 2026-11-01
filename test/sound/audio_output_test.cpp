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

TEST_CASE("Host audio exposes starvation instead of treating inserted zeros as valid sound") {
    Bus bus({});
    AudioOutput output(bus);
    std::array<s16, 960> samples{};
    output.Fill(samples.data(), 480);
    REQUIRE(output.Underruns() > 0);
    REQUIRE(output.OutputFrames() == 480);
}

TEST_CASE("Host AC coupling removes a held DAC offset") {
    Bus bus({});
    bus.Write16(0x04000084, 0x80);
    bus.Write16(0x04000082, 0x0304);
    bus.Write32(0x040000A0, 0x40404040);
    bus.Write16(0x04000100, 0xFFFF);
    bus.Write16(0x04000102, 0x80);
    bus.AdvanceTimers(1);
    bus.Write16(0x04000102, 0);
    bus.AdvanceTimers(512 * 4096);
    AudioOutput output(bus);
    std::array<s16, 960> samples{};
    output.Fill(samples.data(), 480);
    REQUIRE(samples[0] > 0);
    for (unsigned callback = 1; callback < 100; ++callback) {
        bus.AdvanceTimers(512 * 328);
        output.Fill(samples.data(), 480);
    }
    REQUIRE(samples.back() == 0);
    REQUIRE(output.Underruns() == 0);
}

TEST_CASE("Pacing frames from audio demand handles a faster device without changing pitch") {
    Bus bus({});
    AudioOutput output(bus);
    std::array<s16, 816 * 2> samples{};
    for (unsigned callback = 0; callback < 300; ++callback) {
        // Same queue-demand loop as the frontend. 816 output frames per
        // nominal video interval models a device clock about 1.5% faster.
        while (!output.Ready()) bus.AdvanceTimers(Ppu::kCyclesPerFrame);
        output.Fill(samples.data(), 816);
    }
    REQUIRE(output.Underruns() == 0);
    REQUIRE(bus.DroppedAudioSamples() == 0);
    REQUIRE(output.OutputFrames() == 300 * 816);
}
