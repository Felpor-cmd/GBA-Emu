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
