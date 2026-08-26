#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "memory/bus.hpp"

TEST_CASE("ROM reads loaded data in all mirrored address windows") {
    Bus bus(std::vector<u8>{0x99, 0x88, 0x77, 0x66});

    REQUIRE(bus.Read8(0x08000000) == 0x99);
    REQUIRE(bus.Read8(0x0A000000) == 0x99);
    REQUIRE(bus.Read8(0x0C000000) == 0x99);
    REQUIRE(bus.Read8(0x08000004) == 0);
}

TEST_CASE("ROM writes are ignored") {
    Bus bus(std::vector<u8>{0x99, 0x88, 0x77, 0x66});

    bus.Write8(0x08000000, 42);

    REQUIRE(bus.Read8(0x08000000) == 0x99);
}
