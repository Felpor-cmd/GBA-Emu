#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>
#include <vector>

#include "cpu/cpu.hpp"
#include "cpu_test_helpers.hpp"

namespace {
std::vector<u8> ReadFixture(const char* name) {
    std::ifstream input(std::string(FIXTURE_DIR) + "/" + name, std::ios::binary);
    REQUIRE(input.good());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
}

TEST_CASE("Assembled ARM sequence preserves registers flags PC memory and cycles") {
    Bus bus(ReadFixture("sequence.gba"));
    Cpu cpu(bus);
    cpu.SetRegister(2, 0x03000000u);
    const u32 expected_pc[] = {4, 8, 12, 16, 20, 24, 32, 36, 32};
    const u32 expected_clocks[] = {8, 6, 7, 10, 8, 6, 20, 6, 20};
    for (unsigned i = 0; i < 9; ++i) {
        REQUIRE(cpu.Step() == expected_clocks[i]);
        REQUIRE(cpu.GetRegister(15) == 0x08000000u + expected_pc[i]);
    }
    REQUIRE(cpu.GetRegister(0) == 5);
    REQUIRE(cpu.GetRegister(1) == 8);
    REQUIRE(cpu.GetRegister(3) == 8);
    REQUIRE(cpu.GetRegister(4) == 1);
    REQUIRE((cpu.GetCpsr() & 0xF0000000u) == 0x60000000u);
    REQUIRE(bus.Read32(0x03000000u) == 8);
    REQUIRE(cpu.GetCycles() == 91);
}

TEST_CASE("Synthetic BIOS returns from SWI to the following ARM instruction") {
    Bus bus(cpu_test::AsRom(0xEF000000u), ReadFixture("exception_bios.bin"));
    Cpu cpu(bus);
    REQUIRE(cpu.Step() == 10);
    REQUIRE(cpu.GetRegister(15) == 8);
    REQUIRE((cpu.GetCpsr() & 31u) == 0x13u);
    REQUIRE(cpu.Step() == 15);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
    REQUIRE((cpu.GetCpsr() & 31u) == 0x1Fu);
}
