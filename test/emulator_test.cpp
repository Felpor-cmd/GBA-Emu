#include <catch2/catch_test_macros.hpp>
#include "emulator.hpp"

TEST_CASE("CPU ROM writes reach the frame buffer through the shared frame coordinator") {
    std::vector<u8> rom(0x30, 0);
    const u32 code[] = {0xE59F0018, 0xE59F1018, 0xE1C010B0, 0xE59F0014,
        0xE3A0101F, 0xE1C010B0, 0xEAFFFFFE, 0,
        0x04000000, 0x00000403, 0x06000000};
    for (unsigned i = 0; i < 11; ++i)
        for (unsigned byte = 0; byte < 4; ++byte)
            rom[i * 4 + byte] = static_cast<u8>(code[i] >> (byte * 8));
    Emulator emulator(std::move(rom));
    emulator.RunFrame();
    REQUIRE(emulator.Frames() == 1);
    REQUIRE(emulator.Pc() == 0x08000018);
    REQUIRE(emulator.Pixels()[0] == 0xFFFF0000);
    REQUIRE(emulator.Pixels()[1] == 0xFF000000);
    emulator.RunFrame();
    REQUIRE(emulator.Frames() == 2);
    REQUIRE(emulator.Pixels()[0] == 0xFFFF0000);
}
