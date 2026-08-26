#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "memory/bus.hpp"

TEST_CASE("BIOS read uses a zeroed placeholder and ignores writes") {
    Bus bus(std::vector<u8>{0x99, 0x88, 0x77, 0x66});

    REQUIRE(bus.Read8(0x00000123) == 0);

    bus.Write8(0x00000123, 42);
    bus.Write8(0x00000010, 99);

    REQUIRE(bus.Read8(0x00000123) == 0);
    REQUIRE(bus.Read8(0x00000000) == 0);
    REQUIRE(bus.Read8(0x00000010) == 0);
}
