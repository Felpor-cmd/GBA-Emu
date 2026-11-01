#include <catch2/catch_test_macros.hpp>
#include "cpu/cpu.hpp"
#include "memory/bus.hpp"

TEST_CASE("Timer counter reads live count while writes set the reload value") {
    Bus bus({});
    bus.Write16(0x04000100, 0xFFFE);
    bus.Write16(0x04000102, 0x0080);
    bus.AdvanceTimers(1);
    REQUIRE(bus.Read16(0x04000100) == 0xFFFF);

    bus.Write16(0x04000100, 0x1234);
    REQUIRE(bus.Read16(0x04000100) == 0xFFFF);
    bus.AdvanceTimers(1);
    REQUIRE(bus.Read16(0x04000100) == 0x1234);
}

TEST_CASE("Timers honor each system-clock prescaler") {
    constexpr u32 divisors[] = {1, 64, 256, 1024};
    for (unsigned selection = 0; selection < 4; ++selection) {
        Bus bus({});
        bus.Write16(0x04000100, 0);
        bus.Write16(0x04000102, static_cast<u16>(0x0080 | selection));
        bus.AdvanceTimers(divisors[selection] - 1);
        REQUIRE(bus.Read16(0x04000100) == 0);
        bus.AdvanceTimers(1);
        REQUIRE(bus.Read16(0x04000100) == 1);
    }
}

TEST_CASE("Timer overflow reloads and requests its enabled interrupt") {
    Bus bus({});
    bus.Write16(0x04000100, 0xFFFE);
    bus.Write16(0x04000102, 0x00C0);
    bus.AdvanceTimers(1);
    REQUIRE(bus.Read16(0x04000100) == 0xFFFF);
    REQUIRE(bus.Read16(0x04000202) == 0);

    bus.AdvanceTimers(1);
    REQUIRE(bus.Read16(0x04000100) == 0xFFFE);
    REQUIRE(bus.Read16(0x04000202) == 0x0008);
    bus.Write16(0x04000202, 0x0008);
    REQUIRE(bus.Read16(0x04000202) == 0);
}

TEST_CASE("Timer overflow does not request an interrupt when its IRQ bit is clear") {
    Bus bus({});
    bus.Write16(0x04000100, 0xFFFF);
    bus.Write16(0x04000102, 0x0080);
    bus.AdvanceTimers(1);
    REQUIRE(bus.Read16(0x04000100) == 0xFFFF);
    REQUIRE(bus.Read16(0x04000202) == 0);
}

TEST_CASE("Each timer overflow requests its matching interrupt flag") {
    for (unsigned index = 0; index < 4; ++index) {
        Bus bus({});
        const u32 timer = 0x04000100u + index * 4;
        bus.Write16(timer, 0xFFFF);
        bus.Write16(timer + 2, 0x00C0);
        bus.AdvanceTimers(1);
        REQUIRE(bus.Read16(0x04000202) == (1u << (3 + index)));
    }
}

TEST_CASE("Enabled timer IRQ enters the CPU IRQ vector") {
    Bus bus(std::vector<u8>(4));
    Cpu cpu(bus);
    bus.Write16(0x04000200, 0x0008);
    bus.Write16(0x04000208, 1);
    bus.Write16(0x04000100, 0xFFFF);
    bus.Write16(0x04000102, 0x00C0);
    bus.AdvanceTimers(1);

    cpu.Step();

    REQUIRE(cpu.GetRegister(15) == 0x18);
    REQUIRE((cpu.GetCpsr() & 0x1Fu) == 0x12u);
    REQUIRE(bus.Read16(0x04000202) == 0x0008);
}

TEST_CASE("Timer 1 through 3 cascade on the previous timer's overflow") {
    Bus bus({});
    bus.Write16(0x04000100, 0xFFFE);
    bus.Write16(0x04000104, 0xFFFF);
    bus.Write16(0x04000106, 0x00C4);
    bus.Write16(0x04000108, 0xFFFF);
    bus.Write16(0x0400010A, 0x0084);
    bus.Write16(0x0400010C, 0xFFFF);
    bus.Write16(0x0400010E, 0x00C4);
    bus.Write16(0x04000102, 0x00C0);

    bus.AdvanceTimers(1);
    REQUIRE(bus.Read16(0x04000202) == 0);
    bus.AdvanceTimers(1);

    REQUIRE(bus.Read16(0x04000100) == 0xFFFE);
    REQUIRE(bus.Read16(0x04000104) == 0xFFFF);
    REQUIRE(bus.Read16(0x04000108) == 0xFFFF);
    REQUIRE(bus.Read16(0x0400010C) == 0xFFFF);
    REQUIRE(bus.Read16(0x04000202) == 0x0058);
}

TEST_CASE("Timer 0 ignores cascade mode and disabled timers keep their count") {
    Bus bus({});
    bus.Write16(0x04000100, 10);
    bus.Write16(0x04000102, 0x0084);
    bus.AdvanceTimers(3);
    REQUIRE(bus.Read16(0x04000100) == 13);

    bus.Write16(0x04000102, 0x0004);
    bus.AdvanceTimers(8);
    REQUIRE(bus.Read16(0x04000100) == 13);
}

TEST_CASE("Timer reset clears reload, count, and control state") {
    Bus bus({});
    bus.Write16(0x04000100, 0x1234);
    bus.Write16(0x04000102, 0x00C0);
    bus.AdvanceTimers(10);
    bus.ResetMemory(0x80);
    REQUIRE(bus.Read16(0x04000100) == 0);
    REQUIRE(bus.Read16(0x04000102) == 0);
}
