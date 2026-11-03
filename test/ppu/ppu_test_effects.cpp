#include <catch2/catch_test_macros.hpp>
#include "ppu/ppu.hpp"

TEST_CASE("Background alpha blending and brightness operate on five bit channels") {
    Bus bus({});
    Ppu ppu(bus);
    bus.Write16(0x04000000, 0x0300);
    bus.Write16(0x04000008, 0x0100);
    bus.Write16(0x0400000A, 0x0200);
    bus.Write16(0x06000800, 0);
    bus.Write16(0x06001000, 1);
    bus.Write16(0x06000000, 0x1111);
    bus.Write16(0x06000020, 0x2222);
    bus.Write16(0x05000002, 0x001F);
    bus.Write16(0x05000004, 0x7C00);
    bus.Write16(0x04000050, 0x0241); // BG0 alpha with BG1.
    bus.Write16(0x04000052, 0x0808);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFF7B007Bu);
    bus.Write16(0x04000050, 0x0081); // BG0 brighten.
    bus.Write16(0x04000054, 31); // Saturates coefficient to 16.
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFFFFFFu);
    bus.Write16(0x04000050, 0x00C1); // BG0 darken.
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFF000000u);
}

TEST_CASE("Window regions independently select background and effect visibility") {
    Bus bus({});
    Ppu ppu(bus);
    bus.Write16(0x04000000, 0x2100); // BG0, WIN0.
    bus.Write16(0x04000008, 0x0100);
    bus.Write16(0x06000000, 0x1111);
    bus.Write16(0x05000002, 31);
    bus.Write16(0x05000000, 0x7C00);
    bus.Write16(0x04000040, 0x0001); // x in [0,1).
    bus.Write16(0x04000044, 0x0001); // y in [0,1).
    bus.Write16(0x04000048, 1); // BG0 inside, effects disabled.
    bus.Write16(0x0400004A, 0); // Backdrop outside.
    bus.Write16(0x04000050, 0x00C1);
    bus.Write16(0x04000054, 16);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFF0000u);
    REQUIRE(ppu.Pixels()[1] == 0xFF0000FFu);
}

TEST_CASE("Semi transparent OBJ blends with backdrop without first-target selection") {
    Bus bus({});
    Ppu ppu(bus);
    for (u32 object = 1; object < 128; ++object)
        bus.Write16(0x07000000 + object * 8, 0x200);
    bus.Write16(0x04000000, 0x1000);
    bus.Write16(0x07000000, 0x0400);
    bus.Write16(0x06010000, 0x1111);
    bus.Write16(0x05000202, 0x001F);
    bus.Write16(0x05000000, 0x7C00);
    bus.Write16(0x04000050, 0x2000); // Backdrop second target, effect mode zero.
    bus.Write16(0x04000052, 0x0808);
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFF7B007Bu);
}

TEST_CASE("OBJ window reveals its enabled background only under opaque sprite pixels") {
    Bus bus({});
    Ppu ppu(bus);
    for (u32 object = 1; object < 128; ++object)
        bus.Write16(0x07000000u + object * 8, 0x0200u);
    bus.Write16(0x04000000u, 0x9100u); // BG0, OBJ, OBJ window.
    bus.Write16(0x04000008u, 0x0100u); // BG0 map block 1.
    bus.Write8(0x06000000u, 0x11u); // Opaque BG at x=0 and x=1.
    bus.Write16(0x05000000u, 0x7C00u); // Blue backdrop.
    bus.Write16(0x05000002u, 0x001Fu); // Red BG pixel.
    bus.Write16(0x05000202u, 0x03E0u); // Green OBJ pixel must stay hidden.
    bus.Write16(0x07000000u, 0x0800u); // OBJ 0 is an OBJ window.
    bus.Write8(0x06010000u, 0x01u); // Only x=0 is opaque.
    bus.Write16(0x0400004Au, 0x0100u); // BG0 inside OBJ window; backdrop outside.

    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFF0000u);
    REQUIRE(ppu.Pixels()[1] == 0xFF0000FFu);

    bus.Write16(0x04000000u, 0x1100u); // OBJ window disabled.
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFF0000u);
    REQUIRE(ppu.Pixels()[1] == 0xFFFF0000u);

    bus.Write16(0x04000000u, 0x8100u); // OBJ rendering disabled.
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFF0000FFu);
}

TEST_CASE("WIN0 and WIN1 take priority over OBJ window pixels") {
    Bus bus({});
    Ppu ppu(bus);
    for (u32 object = 1; object < 128; ++object)
        bus.Write16(0x07000000u + object * 8, 0x0200u);
    bus.Write16(0x04000000u, 0xF100u); // BG0, OBJ, WIN0, WIN1, OBJ window.
    bus.Write16(0x04000008u, 0x0100u);
    bus.Write8(0x06000000u, 0x11u);
    bus.Write16(0x05000000u, 0x7C00u);
    bus.Write16(0x05000002u, 0x001Fu);
    bus.Write16(0x07000000u, 0x0800u);
    bus.Write8(0x06010000u, 0x11u);
    bus.Write16(0x0400004Au, 0x0100u); // BG0 in OBJ window.
    bus.Write16(0x04000040u, 0x0001u); // WIN0 covers only x=0.
    bus.Write16(0x04000044u, 0x0001u); // WIN0 covers only y=0.
    bus.Write16(0x04000042u, 0x0002u); // WIN1 covers x=0 and x=1.
    bus.Write16(0x04000046u, 0x0001u);
    bus.Write16(0x04000048u, 1u); // BG0 in WIN0, backdrop in WIN1.

    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFF0000u);
    REQUIRE(ppu.Pixels()[1] == 0xFF0000FFu);

    bus.Write16(0x04000000u, 0xD100u); // WIN0 off; WIN1 wins.
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFF0000FFu);

    bus.Write16(0x04000000u, 0x9100u); // Both rectangular windows off.
    ppu.RenderFrame();
    REQUIRE(ppu.Pixels()[0] == 0xFFFF0000u);
}
