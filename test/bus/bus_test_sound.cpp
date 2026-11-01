#include <catch2/catch_test_macros.hpp>
#include "memory/bus.hpp"

namespace {
void EnableDirectSoundA(Bus& bus, u16 control = 0x0300) {
    bus.Write16(0x04000084, 0x0080); // Master sound enable.
    bus.Write16(0x04000082, control); // FIFO A to both outputs, timer 0.
    bus.Write16(0x04000100, 0xFE00);
    bus.Write16(0x04000102, 0x0080); // Timer 0, 32.768 kHz sample rate.
}
}

TEST_CASE("Square PSG channel starts, mixes to both outputs, and stops on length") {
    Bus bus({});
    bus.Write16(0x04000084, 0x0080);
    bus.Write16(0x04000080, 0x1177); // Channel 1 routed left/right, maximum master volume.
    bus.Write16(0x04000082, 0x0002); // PSG at full volume.
    REQUIRE((bus.Read16(0x04000084) & 0x0080) != 0);
    bus.Write16(0x04000062, 0xF0BF); // 50% duty, volume 15, length 1/256 second.
    bus.Write16(0x04000064, 0xC000); // Trigger, length enabled, lowest frequency.

    REQUIRE((bus.Read16(0x04000084) & 1) != 0);
    bus.AdvanceTimers(512);
    s16 left = 0, right = 0;
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(left != 0);
    REQUIRE(right == left);

    bus.AdvanceTimers(32'768);
    REQUIRE((bus.Read16(0x04000084) & 1) == 0);
}

TEST_CASE("All four legacy sound channels expose active status after trigger") {
    Bus bus({});
    bus.Write16(0x04000084, 0x0080);
    bus.Write16(0x04000062, 0xF080);
    bus.Write16(0x04000064, 0x8000);
    bus.Write16(0x04000068, 0xF080);
    bus.Write16(0x0400006C, 0x8000);
    bus.Write16(0x04000070, 0x0080); // Enable wave playback.
    bus.Write16(0x04000090, 0xF0F0);
    bus.Write16(0x04000072, 0x2000); // Full wave volume.
    bus.Write16(0x04000074, 0x8000);
    bus.Write16(0x04000078, 0xF000);
    bus.Write16(0x0400007C, 0x8000);

    REQUIRE((bus.Read16(0x04000084) & 0x000F) == 0x000F);
    bus.Write16(0x04000084, 0);
    REQUIRE((bus.Read16(0x04000084) & 0x008F) == 0);
}

TEST_CASE("Second square, wave, and noise PSG channels generate audible samples") {
    Bus bus({});
    s16 left = 0, right = 0;

    bus.Write16(0x04000084, 0x0080);
    bus.Write16(0x04000080, 0x2070); // Channel 2 left, volume 7.
    bus.Write16(0x04000082, 0x0002);
    bus.Write16(0x04000068, 0xF080);
    bus.Write16(0x0400006C, 0x8000);
    bus.AdvanceTimers(512);
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(left != 0);
    REQUIRE(right == 0);

    bus.Write16(0x04000084, 0); // Stop/reset PSG state before testing channel 3.
    bus.Write16(0x04000084, 0x0080);
    bus.Write16(0x04000080, 0x4070); // Channel 3 left.
    bus.Write16(0x04000082, 0x0002);
    bus.Write16(0x04000070, 0x0040); // Select bank 1 so writes fill playback bank 0.
    bus.Write16(0x04000090, 0xF0F0);
    bus.Write16(0x04000070, 0x0080);
    bus.Write16(0x04000072, 0x2000);
    bus.Write16(0x04000074, 0x8000);
    bus.AdvanceTimers(512);
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(left > 0);
    REQUIRE(right == 0);

    bus.Write16(0x04000084, 0);
    bus.Write16(0x04000084, 0x0080);
    bus.Write16(0x04000080, 0x8070); // Noise channel left.
    bus.Write16(0x04000082, 0x0002);
    bus.Write16(0x04000078, 0xF000);
    bus.Write16(0x0400007C, 0x8000);
    bus.AdvanceTimers(512);
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(left != 0);
    REQUIRE(right == 0);
}

TEST_CASE("High-resolution PWM noise is averaged across each host sample") {
    Bus bus({});
    bus.Write16(0x04000084, 0x0080);
    bus.Write16(0x04000080, 0x8070); // Channel 4 to the left at full volume.
    bus.Write16(0x04000082, 0x0002);
    bus.Write16(0x04000088, 0xC200); // 262 kHz PWM, eight ticks per host sample.
    bus.Write16(0x04000078, 0xF000);
    bus.Write16(0x0400007C, 0x8008); // 7-bit noise, fastest clock.

    bus.AdvanceTimers(512);
    s16 left = 0, right = 0;
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(right == 0);
    REQUIRE(left > -3840);
    REQUIRE(left < 3840);
}

TEST_CASE("Enabled PSG channels mix by summing their panned outputs") {
    const auto sample_with_channels = [](bool include_channel_two) {
        Bus bus({});
        bus.Write16(0x04000084, 0x0080);
        bus.Write16(0x04000080, include_channel_two ? 0x3377 : 0x1177);
        bus.Write16(0x04000082, 0x0002);
        bus.Write16(0x04000062, 0xF080);
        bus.Write16(0x04000064, 0x8000);
        if (include_channel_two) {
            bus.Write16(0x04000068, 0xF080);
            bus.Write16(0x0400006C, 0x8000);
        }
        bus.AdvanceTimers(512);
        s16 left = 0, right = 0;
        REQUIRE(bus.PopAudioSample(left, right));
        return left;
    };

    const s16 one_channel = sample_with_channels(false);
    const s16 two_channels = sample_with_channels(true);
    REQUIRE(two_channels == one_channel * 2);
}

TEST_CASE("Timer overflow clocks signed FIFO A samples into stereo output") {
    Bus bus({});
    EnableDirectSoundA(bus);
    bus.Write32(0x040000A0, 0x00004040); // FIFO order: +64, +64, 0, 0.

    bus.AdvanceTimers(512); // Timer overflow and 32.768 kHz output sample.

    s16 left = 0, right = 0;
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(left > 0);
    REQUIRE(right == left);
}

TEST_CASE("FIFO audio is independent of elapsed-time chunk size") {
    const auto capture = [](u32 chunk) {
        Bus bus({});
        EnableDirectSoundA(bus, 0x0304);
        bus.Write16(0x04000100, 0xFF00);
        bus.Write16(0x04000102, 0);
        bus.Write16(0x04000102, 0x0080);
        bus.Write32(0x040000A0, 0x40302010);
        std::vector<s16> result;
        for (u32 elapsed = 0; elapsed < 1024; elapsed += chunk)
            bus.AdvanceTimers(chunk);
        s16 left = 0, right = 0;
        while (bus.PopAudioSample(left, right)) result.push_back(left);
        return result;
    };
    REQUIRE(capture(1024) == capture(1));
}

TEST_CASE("Running square frequency changes without another trigger") {
    Bus bus({});
    bus.Write16(0x04000084, 0x80);
    bus.Write16(0x04000080, 0x1177);
    bus.Write16(0x04000082, 2);
    bus.Write16(0x04000062, 0xF080);
    bus.Write16(0x04000064, 0x8000);
    bus.AdvanceTimers(512);
    s16 left = 0, right = 0;
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(left > 0);
    bus.Write16(0x04000064, 2040);
    bus.AdvanceTimers(1024);
    // The first sample includes the oscillator's retained fractional phase.
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(left < 0);
}

TEST_CASE("Square duty three is seventy-five percent high") {
    Bus bus({});
    bus.Write16(0x04000084, 0x80);
    bus.Write16(0x04000080, 0x1177);
    bus.Write16(0x04000082, 2);
    bus.Write16(0x04000062, 0xF0C0);
    bus.Write16(0x04000064, 0x8700);
    bus.AdvanceTimers(32768);
    unsigned high = 0, count = 0;
    s16 left = 0, right = 0;
    while (bus.PopAudioSample(left, right)) { ++count; if (left > 0) ++high; }
    REQUIRE(count == 64);
    REQUIRE(high == 48);
}

TEST_CASE("Mixed direct sound clips at the hardware DAC range") {
    Bus bus({});
    EnableDirectSoundA(bus, 0x330C); // Both FIFOs at full volume, timer 0.
    bus.Write32(0x040000A0, 0x7F7F7F7F);
    bus.Write32(0x040000A4, 0x7F7F7F7F);
    bus.AdvanceTimers(512);
    s16 left = 0, right = 0;
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(left == 16320); // clamp 1023, quantize to 9 bits, remove bias 512.
    REQUIRE(right == left);
}

TEST_CASE("Every PWM resolution preserves host duration and applies DAC quantization") {
    const s16 expected[] = {8064, 8064, 7936, 7680};
    for (unsigned mode = 0; mode < 4; ++mode) {
        Bus bus({});
        EnableDirectSoundA(bus, 0x0304);
        bus.Write16(0x04000088, static_cast<u16>(0x0200 | (mode << 14)));
        for (unsigned word = 0; word < 4; ++word) bus.Write32(0x040000A0, 0x3F3F3F3F);
        bus.AdvanceTimers(4096);
        unsigned count = 0;
        s16 left = 0, right = 0;
        while (bus.PopAudioSample(left, right)) {
            if (count > 0) REQUIRE(left == expected[mode]);
            REQUIRE(right == left);
            ++count;
        }
        REQUIRE(count == 8);
    }
}

TEST_CASE("Noise keeps clocking when its period is shortened while running") {
    Bus bus({});
    bus.Write16(0x04000084, 0x80);
    bus.Write16(0x04000080, 0x8877);
    bus.Write16(0x04000082, 2);
    bus.Write16(0x04000078, 0xF000);
    bus.Write16(0x0400007C, 0x80F5);
    bus.AdvanceTimers(512);
    bus.Write8(0x0400007C, 0x08); // New period is shorter than elapsed phase.
    bus.AdvanceTimers(4096);
    s16 left = 0, right = 0, first = 0;
    REQUIRE(bus.PopAudioSample(first, right));
    bool changed = false;
    while (bus.PopAudioSample(left, right)) changed |= left != first;
    REQUIRE(changed);
}

TEST_CASE("Wave RAM accesses the bank opposite the selected playback bank") {
    Bus bus({});
    bus.Write16(0x04000084, 0x80);
    bus.Write16(0x04000070, 0x40);
    bus.Write16(0x04000090, 0xABCD); // Fill bank 0 while stopped.
    bus.Write16(0x04000070, 0);
    REQUIRE(bus.Read16(0x04000090) == 0);
    bus.Write16(0x04000090, 0x1234);
    bus.Write16(0x04000070, 0x40);
    REQUIRE(bus.Read16(0x04000090) == 0xABCD);
}

TEST_CASE("Sound FIFO DMA transfers four words to the fixed FIFO address on request") {
    Bus bus({});
    for (u32 offset = 0; offset < 16; offset += 4)
        bus.Write32(0x02000000 + offset, 0x40404040);

    // DMA1: increment source, fixed FIFO A destination, 32-bit, repeat, special timing.
    bus.Write32(0x040000BC, 0x02000000);
    bus.Write32(0x040000C0, 0x040000A0);
    bus.Write32(0x040000C4, 1);
    bus.Write16(0x040000C6, 0xF640); // Also request the DMA1 completion interrupt.
    EnableDirectSoundA(bus);

    REQUIRE(bus.RunDma(3) == 0); // FIFO DMA only runs on a sound request.
    REQUIRE(bus.AdvanceTimers(512) == 34); // Empty FIFO requests four 32-bit words.
    REQUIRE((bus.Read16(0x040000C6) & 0x8000) != 0); // Repeat DMA stays armed.
    REQUIRE(bus.Read16(0x04000202) == 0x0200);
    s16 left = 0, right = 0;
    REQUIRE(bus.PopAudioSample(left, right)); // The empty FIFO's first sample is silence.
    REQUIRE(left == 0);

    bus.AdvanceTimers(512); // Consume the first byte supplied by DMA.
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(left > 0);
    REQUIRE(right == left);
}

TEST_CASE("FIFO B uses timer 1 and can be routed to only the left output") {
    Bus bus({});
    bus.Write16(0x04000084, 0x0080);
    bus.Write16(0x04000082, 0x6000); // FIFO B, left output, timer 1.
    bus.Write32(0x040000A4, 0x0000C0C0); // Signed -64 sample.
    bus.Write16(0x04000104, 0xFE00);
    bus.Write16(0x04000106, 0x0080);

    bus.AdvanceTimers(512);
    s16 left = 0, right = 1;
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(left < 0);
    REQUIRE(right == 0);
}

TEST_CASE("DMA2 refills FIFO B with four words and requests its completion interrupt") {
    Bus bus({});
    for (u32 offset = 0; offset < 16; offset += 4)
        bus.Write32(0x02000000 + offset, 0xC0C0C0C0);
    bus.Write32(0x040000C8, 0x02000000);
    bus.Write32(0x040000CC, 0x040000A4);
    bus.Write32(0x040000D0, 1);
    bus.Write16(0x040000D2, 0xF640);
    bus.Write16(0x04000084, 0x0080);
    bus.Write16(0x04000082, 0x6000); // FIFO B, left output, timer 1.
    bus.Write16(0x04000104, 0xFE00);
    bus.Write16(0x04000106, 0x0080);

    REQUIRE(bus.AdvanceTimers(512) == 34);
    REQUIRE(bus.Read16(0x04000202) == 0x0400);
    s16 left = 1, right = 1;
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(left == 0);
    REQUIRE(right == 0);
    bus.AdvanceTimers(512);
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(left < 0);
    REQUIRE(right == 0);
}

TEST_CASE("Sound control reset clears FIFO data before timer playback") {
    Bus bus({});
    EnableDirectSoundA(bus);
    bus.Write32(0x040000A0, 0x00004040);
    bus.Write8(0x04000083, 0x08); // SOUNDCNT_H reset FIFO A.
    bus.AdvanceTimers(512);

    s16 left = 1, right = 1;
    REQUIRE(bus.PopAudioSample(left, right));
    REQUIRE(left == 0);
    REQUIRE(right == 0);
}
