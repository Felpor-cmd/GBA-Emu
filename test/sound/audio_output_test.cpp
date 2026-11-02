#include <array>
#include <algorithm>
#include <cstdlib>
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

TEST_CASE("Fast-forward audio consumes emulated samples faster and recovers at normal speed") {
    Bus bus({});
    AudioOutput output(bus);
    bus.AdvanceTimers(Ppu::kCyclesPerFrame * 12);
    std::array<s16, 480 * 2> samples{};
    output.SetPlaybackSpeedPercent(200);
    const u32 before = bus.QueuedAudioSamples();
    output.Fill(samples.data(), 480);
    const u32 consumed_fast = before - bus.QueuedAudioSamples();
    REQUIRE(consumed_fast >= 655);
    REQUIRE(consumed_fast <= 657);
    output.SetPlaybackSpeedPercent(100);
    output.Fill(samples.data(), 480);
    REQUIRE(bus.QueuedAudioSamples() <= 2048);
}

TEST_CASE("Fast-forward audio handles the 10x setting without dropping samples") {
    Bus bus({});
    AudioOutput output(bus);
    bus.AdvanceTimers(Ppu::kCyclesPerFrame * 24);
    std::array<s16, 480 * 2> samples{};
    output.SetPlaybackSpeedPercent(1000);
    const u32 before = bus.QueuedAudioSamples();
    output.Fill(samples.data(), 480);
    const u32 consumed = before - bus.QueuedAudioSamples();
    REQUIRE(consumed >= 3277);
    REQUIRE(consumed <= 3280);
    REQUIRE(output.Underruns() == 0);
    REQUIRE(bus.DroppedAudioSamples() == 0);
}

TEST_CASE("Normal-speed square audio does not clip during interpolation") {
    Bus bus({});
    bus.Write16(0x04000084, 0);
    bus.Write16(0x04000084, 0x80);
    bus.Write16(0x04000088, 0x0200);
    bus.Write16(0x04000082, 2);
    bus.Write16(0x04000080, 0x1177);
    bus.Write16(0x04000062, 0xF080);
    bus.Write16(0x04000064, 0x8600);
    bus.AdvanceTimers(Ppu::kCyclesPerFrame * 5);
    AudioOutput output(bus);
    std::array<s16, 1024 * 2> samples{};
    output.Fill(samples.data(), 1024);
    const auto peak = std::max_element(samples.begin(), samples.end(),
        [](s16 a, s16 b) { return std::abs(static_cast<int>(a)) < std::abs(static_cast<int>(b)); });
    REQUIRE(std::abs(static_cast<int>(*peak)) < 30000);
}
