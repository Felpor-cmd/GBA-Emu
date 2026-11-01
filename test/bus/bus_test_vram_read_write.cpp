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

TEST_CASE("VRAM preserves the distinct 32K block at offset 8000") {
    Bus bus({});
    bus.Write16(0x06000000, 0x1111);
    bus.Write16(0x06008000, 0x2222);
    bus.Write16(0x06010000, 0x3333);
    REQUIRE(bus.Read16(0x06000000) == 0x1111);
    REQUIRE(bus.Read16(0x06008000) == 0x2222);
    REQUIRE(bus.Read16(0x06010000) == 0x3333);
    REQUIRE(bus.Read16(0x06018000) == 0x3333);
    REQUIRE(bus.Read16(0x06028000) == 0x2222);
}
