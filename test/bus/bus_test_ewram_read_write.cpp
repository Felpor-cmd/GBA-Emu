#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "memory/bus.hpp"

TEST_CASE("EWRAM read/write") {
    Bus bus(std::vector<u8>{});
    const u32 base = 0x02000000;

    bus.Write8(base + 0x1234, 42);

    REQUIRE(bus.Read8(base + 0x1234) == 42);
}

TEST_CASE("EWRAM edge addresses") {
    Bus bus(std::vector<u8>{});
    const u32 base = 0x02000000;
    const u32 last = base + kEwramSize - 1;

    bus.Write8(base, 0xAA);
    bus.Write8(last, 0xBB);

    REQUIRE(bus.Read8(base) == 0xAA);
    REQUIRE(bus.Read8(last) == 0xBB);
}

TEST_CASE("EWRAM mirroring") {
    Bus bus(std::vector<u8>{});
    const u32 base = 0x02000000;
    const u32 out_of_bounds = base + kEwramSize + 0x10;
    const u32 wrapped = base + ((out_of_bounds - base) & (kEwramSize - 1));

    bus.Write8(out_of_bounds, 0x55);

    REQUIRE(bus.Read8(out_of_bounds) == 0x55);
    REQUIRE(bus.Read8(wrapped) == 0x55);
}

TEST_CASE("EWRAM 32-bit values use little-endian byte order") {
    Bus bus(std::vector<u8>{});
    const u32 address = 0x02000000;

    bus.Write32(address, 0x12345678);

    REQUIRE(bus.Read8(address + 0) == 0x78);
    REQUIRE(bus.Read8(address + 1) == 0x56);
    REQUIRE(bus.Read8(address + 2) == 0x34);
    REQUIRE(bus.Read8(address + 3) == 0x12);
}
