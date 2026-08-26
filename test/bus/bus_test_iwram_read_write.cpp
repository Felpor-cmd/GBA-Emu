#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "memory/bus.hpp"

TEST_CASE("IWRAM read/write") {
    Bus bus(std::vector<u8>{});
    const u32 base = 0x03000000;

    bus.Write8(base + 0x456, 42);

    REQUIRE(bus.Read8(base + 0x456) == 42);
}

TEST_CASE("IWRAM edge addresses") {
    Bus bus(std::vector<u8>{});
    const u32 base = 0x03000000;
    const u32 last = base + kIwramSize - 1;

    bus.Write8(base, 0xAA);
    bus.Write8(last, 0xBB);

    REQUIRE(bus.Read8(base) == 0xAA);
    REQUIRE(bus.Read8(last) == 0xBB);
}

TEST_CASE("IWRAM mirroring") {
    Bus bus(std::vector<u8>{});
    const u32 base = 0x03000000;
    const u32 out_of_bounds = base + kIwramSize + 0x20;
    const u32 wrapped = base + ((out_of_bounds - base) & (kIwramSize - 1));

    bus.Write8(out_of_bounds, 0x55);

    REQUIRE(bus.Read8(out_of_bounds) == 0x55);
    REQUIRE(bus.Read8(wrapped) == 0x55);
}
