#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "memory/bus.hpp"

TEST_CASE("I/O register placeholder read/write") {
    Bus bus(std::vector<u8>{});
    const u32 address = 0x04000010;

    bus.Write8(address, 0x5A);

    REQUIRE(bus.Read8(address) == 0x5A);
}
