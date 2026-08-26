#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "memory/bus.hpp"

TEST_CASE("VRAM read/write") {
    Bus bus(std::vector<u8>{});
    const u32 base = 0x06000000;

    bus.Write8(base + 0x5678, 42);

    REQUIRE(bus.Read8(base + 0x5678) == 42);
}

TEST_CASE("VRAM edge addresses") {
    Bus bus(std::vector<u8>{});
    const u32 base = 0x06000000;
    const u32 last = base + kVramSize - 1;

    bus.Write8(base, 0xAA);
    bus.Write8(last, 0xBB);

    REQUIRE(bus.Read8(base) == 0xAA);
    REQUIRE(bus.Read8(last) == 0xBB);
}

TEST_CASE("VRAM mirroring") {
    Bus bus(std::vector<u8>{});
    const u32 base = 0x06000000;
    const u32 out_of_bounds = base + kVramSize + 0x40;
    const u32 wrapped = base + ((out_of_bounds - base) & (kVramSize - 1));

    bus.Write8(out_of_bounds, 0x55);

    REQUIRE(bus.Read8(out_of_bounds) == 0x55);
    REQUIRE(bus.Read8(wrapped) == 0x55);
}
