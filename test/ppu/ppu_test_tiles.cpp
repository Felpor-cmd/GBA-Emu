#include <catch2/catch_test_macros.hpp>

#include "memory/bus.hpp"
#include "ppu/ppu.hpp"

TEST_CASE("Text backgrounds use map palette and priority in mode zero") {
    Bus bus({});
    Ppu ppu(bus);
    bus.Write16(0x04000000, 0x0300); // BG0 and BG1.
    bus.Write16(0x04000008, 0x0101); // BG0 map block 1, priority 1.
    bus.Write16(0x0400000A, 0x0200); // BG1 map block 2, priority 0.
    bus.Write8(0x06000000, 0x01);   // Tile zero, first pixel index 1.
    bus.Write16(0x06000800, 0x0000); // BG0 palette bank 0.
    bus.Write16(0x06001000, 0x1000); // BG1 palette bank 1.
    bus.Write16(0x05000002, 0x001F); // Red.
    bus.Write16(0x05000022, 0x03E0); // Green.
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFF00FF00u);

    bus.Write16(0x0400000A, 0x0202);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFF0000u);
}

TEST_CASE("Affine background uses matrix coordinates and overflow wrapping") {
    Bus bus({});
    Ppu ppu(bus);
    bus.Write16(0x04000000, 0x0402); // Mode 2, BG2.
    bus.Write16(0x0400000C, 0x0100); // Map block 1, 128x128.
    bus.Write8(0x06000800, 0);      // Tile zero.
    bus.Write8(0x06000000, 1);      // Top left pixel.
    bus.Write16(0x05000002, 0x001F);
    bus.Write16(0x04000020, 0x0100); // Identity PA.
    bus.Write16(0x04000026, 0x0100); // Identity PD.
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFF0000u);

    bus.Write32(0x04000028, 128u << 8);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFF000000u);
    bus.Write16(0x0400000C, 0x2100); // Wrap area.
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFF0000u);
}

TEST_CASE("Mode one combines text BG0 with affine BG2 by priority") {
    Bus bus({});
    Ppu ppu(bus);
    bus.Write16(0x04000000, 0x0501); // Mode 1, BG0 and BG2.
    bus.Write16(0x04000008, 0x0101); // BG0 map 1, priority 1.
    bus.Write16(0x0400000C, 0x0200); // BG2 map 2, priority 0.
    bus.Write8(0x06000000, 0x01);   // Text tile 0, index 1.
    bus.Write8(0x06000040, 2);      // Affine tile 1, index 2.
    bus.Write8(0x06001000, 1);      // BG2 map references tile 1.
    bus.Write16(0x05000002, 0x001F);
    bus.Write16(0x05000004, 0x03E0);
    bus.Write16(0x04000020, 0x0100);
    bus.Write16(0x04000026, 0x0100);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFF00FF00u);
    bus.Write16(0x0400000C, 0x0202);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFF0000u);
}

TEST_CASE("Regular OBJ appears above equal-priority background and below higher priority") {
    Bus bus({});
    Ppu ppu(bus);
    for (u32 object = 1; object < 128; ++object)
        bus.Write16(0x07000000u + object * 8, 0x0200);
    bus.Write16(0x04000000, 0x1100); // BG0 and OBJ.
    bus.Write16(0x04000008, 0x0100); // BG0 map 1, priority 0.
    bus.Write8(0x06000000, 1);
    bus.Write16(0x05000002, 0x001F); // BG red.
    bus.Write8(0x06010000, 2);       // OBJ palette index 2.
    bus.Write16(0x05000204, 0x7C00); // OBJ blue.
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFF0000FFu);
    bus.Write16(0x07000004, 0x0400); // OBJ priority 1.
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFF0000u);
}

TEST_CASE("Affine OBJ reads its OAM matrix") {
    Bus bus({});
    Ppu ppu(bus);
    bus.Write16(0x04000000, 0x1000); // OBJ only.
    bus.Write16(0x07000000, 0x0100); // OBJ0 affine.
    bus.Write16(0x07000006, 0x0100); // PA.
    bus.Write16(0x0700001E, 0x0100); // PD.
    bus.Write8(0x06010000, 1);
    bus.Write16(0x05000202, 0x03E0);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFF00FF00u);
}

TEST_CASE("Earlier OAM entry wins over later OBJ even with lower BG priority") {
    Bus bus({});
    Ppu ppu(bus);
    for (u32 object = 2; object < 128; ++object)
        bus.Write16(0x07000000u + object * 8, 0x0200);
    bus.Write16(0x04000000, 0x1000);
    bus.Write16(0x07000004, 0x0400); // OBJ0 priority 1, tile 0.
    bus.Write16(0x0700000C, 0x0001); // OBJ1 priority 0, tile 1.
    bus.Write8(0x06010000, 1);
    bus.Write8(0x06010020, 2);
    bus.Write16(0x05000202, 0x001F);
    bus.Write16(0x05000204, 0x7C00);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFF0000u);
}
