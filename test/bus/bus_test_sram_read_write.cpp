#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "memory/bus.hpp"

TEST_CASE("SRAM read/write") {
    Bus bus(std::vector<u8>{});
    const u32 address = 0x0E000123;

    bus.Write8(address, 0xA5);

    REQUIRE(bus.Read8(address) == 0xA5);
}
