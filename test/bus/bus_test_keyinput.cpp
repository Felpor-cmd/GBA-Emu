#include <catch2/catch_test_macros.hpp>
#include "memory/bus.hpp"

TEST_CASE("KEYINPUT starts with every GBA button released") {
    Bus bus({});
    REQUIRE(bus.Read16(0x04000130) == 0x03FF);
}

TEST_CASE("KEYINPUT exposes pressed buttons as active-low bits and ignores writes") {
    Bus bus({});
    bus.SetKeyInput((1u << 0) | (1u << 3) | (1u << 9));
    REQUIRE(bus.Read16(0x04000130) == 0x01F6);
    bus.Write16(0x04000130, 0);
    REQUIRE(bus.Read16(0x04000130) == 0x01F6);
    bus.SetKeyInput(0);
    REQUIRE(bus.Read16(0x04000130) == 0x03FF);
}
