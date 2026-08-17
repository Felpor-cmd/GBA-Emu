#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include "cpu/cpu.hpp"

TEST_CASE("Cpu::CheckCondition") {
    struct TestCase { u32 cond; u32 cpsr; bool expected; const char* name; };
    TestCase tests[] = {
        {0x0, 0x40000000, true,  "EQ with Z=1"},
        {0x0, 0x00000000, false, "EQ with Z=0"},
        {0xA, 0x00000000, true,  "GE with N=0,V=0"},
        {0xA, 0x80000000, false, "GE with N=1,V=0"},
        {0xE, 0x00000000, true,  "AL always true"},
    };
    for (auto& t : tests) {
        bool result = Cpu::CheckCondition(t.cond, t.cpsr);
        std::printf("%-16s expected=%d got=%d %s\n",
            t.name, t.expected, result, result == t.expected ? "OK" : "MISMATCH");
        REQUIRE(result == t.expected);
    }
}