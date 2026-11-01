#include <catch2/catch_test_macros.hpp>
#include "memory/bus.hpp"

TEST_CASE("Immediate DMA3 copies halfwords clears enable and requests acknowledged IRQ") {
    Bus bus({});
    bus.Write32(0x02000000, 0x12345678);
    bus.Write32(0x040000D4, 0x02000000);
    bus.Write32(0x040000D8, 0x03000000);
    bus.Write32(0x040000DC, 0xC0000002); // IRQ, enable, 2 halfwords.
    REQUIRE(bus.Read32(0x03000000) == 0);
    REQUIRE(bus.RunDma(1) == 0);
    REQUIRE(bus.RunDma(0) == 10); // 2 setup + 2*(3 EWRAM + 1 IWRAM).
    REQUIRE(bus.Read32(0x03000000) == 0x12345678);
    REQUIRE((bus.Read16(0x040000DE) & 0x8000) == 0);
    REQUIRE(bus.Read16(0x04000202) == 0x800);
    REQUIRE_FALSE(bus.InterruptPending());
    bus.Write16(0x04000200, 0x800);
    REQUIRE(bus.InterruptPending());
    bus.Write8(0x04000202, 0xFF); // Low byte acknowledgement cannot clear DMA3.
    REQUIRE(bus.InterruptPending());
    bus.Write16(0x04000202, 0x800);
    REQUIRE_FALSE(bus.InterruptPending());
    REQUIRE(bus.RunDma(0) == 0);
}

TEST_CASE("Repeated VBlank DMA reloads destination and continues the source") {
    Bus bus({});
    bus.Write32(0x02000000, 0x11112222);
    bus.Write32(0x02000004, 0x33334444);
    bus.Write32(0x040000D4, 0x02000000);
    bus.Write32(0x040000D8, 0x03000000);
    bus.Write32(0x040000DC, 0x96600001); // Word, repeat, VBlank, destination reload.
    REQUIRE(bus.RunDma(0) == 0);
    bus.RunDma(1);
    REQUIRE(bus.Read32(0x03000000) == 0x11112222);
    REQUIRE(bus.Read16(0x040000DE) & 0x8000);
    bus.RunDma(1);
    REQUIRE(bus.Read32(0x03000000) == 0x33334444);
    REQUIRE(bus.Read32(0x03000004) == 0);
}

TEST_CASE("HBlank DMA runs on its selected trigger and honours channel count limits") {
    Bus bus({});
    bus.Write32(0x02000000, 0x12345678);
    bus.Write32(0x040000B0, 0x02000000);
    bus.Write32(0x040000B4, 0x03000000);
    bus.Write32(0x040000B8, 0xA4000001); // DMA0, word, HBlank, one transfer.
    REQUIRE(bus.RunDma(0) == 0);
    REQUIRE(bus.RunDma(1) == 0);
    REQUIRE(bus.Read32(0x03000000) == 0);
    REQUIRE(bus.RunDma(2) > 0);
    REQUIRE(bus.Read32(0x03000000) == 0x12345678);
    REQUIRE(bus.RunDma(2) == 0);
}

TEST_CASE("DMA supports fixed source and decreasing destination") {
    Bus bus({});
    bus.Write32(0x02000000, 0xDEADBEEF);
    bus.Write32(0x040000D4, 0x02000000);
    bus.Write32(0x040000D8, 0x03000004);
    bus.Write32(0x040000DC, 0x85200002);
    bus.RunDma(0);
    REQUIRE(bus.Read32(0x03000000) == 0xDEADBEEF);
    REQUIRE(bus.Read32(0x03000004) == 0xDEADBEEF);
}

TEST_CASE("RegisterRamReset independently restores I/O groups without erasing keypad or stack") {
    Bus bus({});
    bus.Write16(0x04000008, 0xFFFF);
    bus.Write16(0x04000134, 0x1234);
    bus.Write16(0x04000088, 0x1234);
    bus.Write16(0x04000120, 0x1234);
    bus.ResetMemory(0);
    REQUIRE(bus.Read16(0x04000000) == 0x80);
    REQUIRE(bus.Read16(0x04000008) == 0xFFFF);
    REQUIRE(bus.Read16(0x04000120) == 0);
    bus.ResetMemory(0x20);
    REQUIRE(bus.Read16(0x04000134) == 0x8000);
    REQUIRE(bus.Read16(0x04000088) == 0x1234);
    bus.ResetMemory(0x40);
    REQUIRE(bus.Read16(0x04000088) == 0x200);
    REQUIRE(bus.Read16(0x04000008) == 0xFFFF);
    bus.ResetMemory(0x80);
    REQUIRE(bus.Read16(0x04000008) == 0);
    REQUIRE(bus.Read16(0x04000020) == 0x100);
    REQUIRE(bus.Read16(0x04000026) == 0x100);
    REQUIRE(bus.Read16(0x04000130) == 0x3FF);
}
