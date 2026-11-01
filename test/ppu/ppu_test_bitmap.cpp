#include <catch2/catch_test_macros.hpp>

#include "memory/bus.hpp"
#include "ppu/ppu.hpp"

TEST_CASE("Bitmap mode 3 displays GBA BGR555 pixels and respects BG2 enable") {
    Bus bus({});
    Ppu ppu(bus);
    bus.Write16(0x05000000, 0x03E0); // Green backdrop.
    bus.Write16(0x06000000, 0x001F); // Red at top left.
    bus.Write16(0x06000000 + (159 * 240 + 239) * 2, 0x7C00); // Blue at bottom right.
    bus.Write16(0x04000000, 3);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFF00FF00u);

    bus.Write16(0x04000000, 0x0403);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFF0000u);
    REQUIRE(ppu.Pixels().back() == 0xFF0000FFu);
}

TEST_CASE("Bitmap mode 4 selects a page and treats palette zero as backdrop") {
    Bus bus({});
    Ppu ppu(bus);
    bus.Write16(0x05000000, 0x7C00); // Blue backdrop.
    bus.Write16(0x05000002, 0x001F); // Red palette entry 1.
    bus.Write16(0x05000004, 0x03E0); // Green palette entry 2.
    bus.Write8(0x06000000, 1);
    bus.Write8(0x0600A000, 2);
    bus.Write16(0x04000000, 0x0404);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFF0000u);
    REQUIRE(ppu.Pixels()[1] == 0xFF0000FFu);

    bus.Write16(0x04000000, 0x0414);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFF00FF00u);
}

TEST_CASE("Bitmap mode 5 uses 160 by 128 true color pages and backdrop outside") {
    Bus bus({});
    Ppu ppu(bus);
    bus.Write16(0x05000000, 0x7C00); // Blue backdrop.
    bus.Write16(0x06000000, 0x001F);
    bus.Write16(0x0600A000, 0x03E0);
    bus.Write16(0x04000000, 0x0405);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFF0000u);
    REQUIRE(ppu.Pixels()[160] == 0xFF0000FFu);
    REQUIRE(ppu.Pixels()[128 * 240] == 0xFF0000FFu);

    bus.Write16(0x04000000, 0x0415);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFF00FF00u);
}

TEST_CASE("Forced blank displays white") {
    Bus bus({});
    Ppu ppu(bus);
    bus.Write16(0x04000000, 0x0483);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFFFFFFu);
    REQUIRE(ppu.Pixels().back() == 0xFFFFFFFFu);
}

TEST_CASE("PPU advances HBlank VBlank and VCOUNT at GBA scanline boundaries") {
    Bus bus({});
    Ppu ppu(bus);
    bus.Write16(0x04000004, 160u << 8); // VCOUNT comparison.
    ppu.Advance(1006);
    REQUIRE((bus.Read16(0x04000004) & 3u) == 2u);
    ppu.Advance(226);
    REQUIRE(bus.Read16(0x04000006) == 1);
    REQUIRE((bus.Read16(0x04000004) & 3u) == 0u);
    ppu.Advance(159u * 1232u);
    REQUIRE(bus.Read16(0x04000006) == 160);
    REQUIRE((bus.Read16(0x04000004) & 5u) == 5u);
    ppu.Advance(67u * 1232u);
    REQUIRE(bus.Read16(0x04000006) == 227);
    REQUIRE((bus.Read16(0x04000004) & 1u) == 0u);
    ppu.Advance(1232u);
    REQUIRE(bus.Read16(0x04000006) == 0);
    REQUIRE((bus.Read16(0x04000004) & 5u) == 0u);
}

TEST_CASE("Video IRQs use status enables and HBlank DMA excludes hidden scanlines") {
    Bus bus({});
    Ppu ppu(bus);
    bus.Write16(0x04000004u, 0xA038); // All video IRQs, compare line 160.
    REQUIRE((ppu.Advance(1005) & 2u) == 0);
    REQUIRE(bus.Read16(0x04000202u) == 0);
    REQUIRE((ppu.Advance(1) & 2u) != 0);
    REQUIRE(bus.Read16(0x04000202u) == 2);
    ppu.Advance(226 + 159 * 1232);
    REQUIRE(bus.Read16(0x04000006u) == 160);
    REQUIRE((bus.Read16(0x04000202u) & 7u) == 7u);
    bus.Write16(0x04000202u, 7);
    REQUIRE((ppu.Advance(1006) & 2u) == 0);
    REQUIRE(bus.Read16(0x04000202u) == 2); // IRQ still fires in hidden HBlank.
}
