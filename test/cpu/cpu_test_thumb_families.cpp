#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <tuple>
#include <utility>
#include <vector>

#include "cpu_test_helpers.hpp"

namespace {
constexpr u32 kN = 1u << 31, kZ = 1u << 30, kC = 1u << 29, kV = 1u << 28;

std::vector<u8> Rom(std::initializer_list<u16> instructions) {
    std::vector<u8> rom(64);
    size_t offset = 0;
    for (u16 instruction : instructions) {
        rom[offset++] = static_cast<u8>(instruction);
        rom[offset++] = static_cast<u8>(instruction >> 8);
    }
    return rom;
}

void EnterThumb(Cpu& cpu, u32 flags = 0) {
    cpu.SetCpsr(0x1Fu | flags);
    cpu.SetRegister(0, 0x08000001u);
    cpu.ExecuteBranchExchange(0xE12FFF10u);
    cpu.SetRegister(15, 0x08000000u);
}

u16 Alu(u32 op, u32 rs = 1, u32 rd = 0) {
    return static_cast<u16>(0x4000u | (op << 6) | (rs << 3) | rd);
}
}

TEST_CASE("Thumb immediate arithmetic covers carry overflow and unchanged MOV carry") {
    Bus bus(Rom({0x20FF, 0x3001, 0x2800, 0x3801}));
    Cpu cpu(bus);
    EnterThumb(cpu, kC | kV);
    REQUIRE(cpu.Step() == 5);
    REQUIRE(cpu.GetRegister(0) == 255);
    REQUIRE((cpu.GetCpsr() & (kC | kV)) == (kC | kV));
    cpu.Step();
    REQUIRE(cpu.GetRegister(0) == 256);
    REQUIRE((cpu.GetCpsr() & (kC | kV)) == 0);
    cpu.Step();
    REQUIRE((cpu.GetCpsr() & kC) != 0);
    cpu.Step();
    REQUIRE(cpu.GetRegister(0) == 255);
}

TEST_CASE("Thumb ALU register shifts use the low byte and ARM7 carry boundaries") {
    for (auto [amount, value, expected, expected_carry] : {
             std::tuple{0u, 0x80000001u, 0x80000001u, true},
             std::tuple{32u, 0x80000001u, 0u, true},
             std::tuple{33u, 0x80000001u, 0u, false},
             std::tuple{256u, 0x80000001u, 0x80000001u, true}}) {
        Bus bus(Rom({Alu(2)})); Cpu cpu(bus); EnterThumb(cpu, kC | kV);
        cpu.SetRegister(0, value); cpu.SetRegister(1, amount);
        REQUIRE(cpu.Step() == 6);
        REQUIRE(cpu.GetRegister(0) == expected);
        REQUIRE(((cpu.GetCpsr() & kC) != 0) == expected_carry);
        REQUIRE((cpu.GetCpsr() & kV) != 0);
    }
    Bus bus(Rom({Alu(7)})); Cpu cpu(bus); EnterThumb(cpu);
    cpu.SetRegister(0, 0x80000001u); cpu.SetRegister(1, 32);
    cpu.Step();
    REQUIRE(cpu.GetRegister(0) == 0x80000001u);
    REQUIRE((cpu.GetCpsr() & kC) != 0);
}

TEST_CASE("Thumb ALU arithmetic and logic set their specified flags") {
    for (u32 op : {5u, 6u, 9u, 10u, 11u}) {
        Bus bus(Rom({Alu(op)})); Cpu cpu(bus); EnterThumb(cpu, kC);
        cpu.SetRegister(0, 0x7FFFFFFFu); cpu.SetRegister(1, 1);
        cpu.Step();
        if (op == 5 || op == 11) REQUIRE((cpu.GetCpsr() & kV) != 0);
        if (op == 10) REQUIRE(cpu.GetRegister(0) == 0x7FFFFFFFu);
    }
    for (u32 op : {0u, 1u, 8u, 12u, 14u, 15u}) {
        Bus bus(Rom({Alu(op)})); Cpu cpu(bus); EnterThumb(cpu, kC | kV);
        cpu.SetRegister(0, 0xF0u); cpu.SetRegister(1, 0x0Fu);
        cpu.Step();
        REQUIRE((cpu.GetCpsr() & (kC | kV)) == (kC | kV));
    }
}

TEST_CASE("Thumb multiply charges iterations from the incoming destination") {
    Bus bus(Rom({Alu(13)})); Cpu cpu(bus); EnterThumb(cpu);
    cpu.SetRegister(0, 0x10000u); cpu.SetRegister(1, 2);
    REQUIRE(cpu.Step() == 8);
    REQUIRE(cpu.GetRegister(0) == 0x20000u);
}

TEST_CASE("Thumb register shift and multiply restart ROM fetch sequencing") {
    for (u16 instruction : {Alu(2), Alu(13)}) {
        Bus bus(Rom({instruction, 0x2001u})); Cpu cpu(bus); EnterThumb(cpu);
        cpu.SetRegister(0, 2); cpu.SetRegister(1, 1);
        cpu.Step();
        REQUIRE(cpu.Step() == 5);
    }
}

TEST_CASE("Thumb high register operations read PC plus four and refill on PC writes") {
    Bus bus(Rom({0x4478u})); Cpu cpu(bus); EnterThumb(cpu);
    cpu.SetRegister(0, 1);
    cpu.Step(); // ADD r0,pc (source high PC)
    REQUIRE(cpu.GetRegister(0) == 0x08000005u);

    Bus bus2(Rom({0x4687u})); Cpu cpu2(bus2); EnterThumb(cpu2);
    cpu2.SetRegister(0, 0x03000003u);
    REQUIRE(cpu2.Step() == 7);
    REQUIRE(cpu2.GetRegister(15) == 0x03000002u);
    REQUIRE((cpu2.GetCpsr() & kThumbFlag) != 0);
}

TEST_CASE("Thumb PC relative literal aligns PC at halfword offset two") {
    auto rom = Rom({0x46C0u, 0x4800u});
    cpu_test::Put32(rom, 4, 0x12345678u);
    Bus bus(rom); Cpu cpu(bus); EnterThumb(cpu);
    cpu.Step(); cpu.Step();
    REQUIRE(cpu.GetRegister(0) == 0x12345678u);
}

TEST_CASE("Thumb register offset transfers handle signed and unaligned data") {
    for (auto [op, expected] : {std::pair{3u, 0xFFFFFF80u},
                                std::pair{5u, 0x00008080u},
                                std::pair{6u, 0x00000080u},
                                std::pair{7u, 0xFFFFFF80u}}) {
        Bus bus(Rom({static_cast<u16>(0x5000u | (op << 9) | (2u << 6) | (1u << 3))}));
        Cpu cpu(bus); EnterThumb(cpu);
        cpu.SetRegister(1, 0x03000000u); cpu.SetRegister(2, 1u);
        bus.Write16(0x03000000u, 0x8080u);
        cpu.Step();
        REQUIRE(cpu.GetRegister(0) == expected);
    }
    Bus bus(Rom({0x5888u})); Cpu cpu(bus); EnterThumb(cpu);
    cpu.SetRegister(1, 0x03000000u); cpu.SetRegister(2, 1u);
    bus.Write32(0x03000000u, 0x12345678u);
    cpu.Step();
    REQUIRE(cpu.GetRegister(0) == 0x78123456u);
}

TEST_CASE("Thumb immediate word byte halfword and SP transfers access memory") {
    for (auto [store, load, base] : {std::tuple{0x6048u, 0x684Au, 0x03000000u},
                                     std::tuple{0x7048u, 0x784Au, 0x03000000u},
                                     std::tuple{0x8048u, 0x884Au, 0x03000000u},
                                     std::tuple{0x9001u, 0x9A01u, 0x03000000u}}) {
        Bus bus(Rom({static_cast<u16>(store), static_cast<u16>(load)}));
        Cpu cpu(bus); EnterThumb(cpu);
        cpu.SetRegister(0, 0x12345678u);
        cpu.SetRegister(1, base);
        cpu.SetRegister(13, base);
        cpu.Step(); cpu.Step();
        REQUIRE(cpu.GetRegister(2) == ((store == 0x7048u) ? 0x78u :
                                       (store == 0x8048u) ? 0x5678u : 0x12345678u));
    }
}

TEST_CASE("Thumb address generation and SP adjustment preserve flags") {
    Bus bus(Rom({0xA001u, 0xA901u, 0xB081u, 0xB001u})); Cpu cpu(bus);
    EnterThumb(cpu, kC | kV);
    cpu.SetRegister(13, 0x03000100u);
    cpu.Step(); REQUIRE(cpu.GetRegister(0) == 0x08000008u);
    cpu.Step(); REQUIRE(cpu.GetRegister(1) == 0x03000104u);
    cpu.Step(); REQUIRE(cpu.GetRegister(13) == 0x030000FCu);
    cpu.Step(); REQUIRE(cpu.GetRegister(13) == 0x03000100u);
    REQUIRE((cpu.GetCpsr() & (kC | kV)) == (kC | kV));
}

TEST_CASE("Thumb PUSH and POP order registers and preserve Thumb on PC load") {
    Bus bus(Rom({0xB501u, 0xBD01u})); Cpu cpu(bus); EnterThumb(cpu);
    cpu.SetRegister(0, 0x12345678u);
    cpu.SetRegister(14, 0x03000011u);
    cpu.SetRegister(13, 0x03000100u);
    REQUIRE(cpu.Step() == 7);
    REQUIRE(cpu.GetRegister(13) == 0x030000F8u);
    REQUIRE(bus.Read32(0x030000F8u) == 0x12345678u);
    REQUIRE(bus.Read32(0x030000FCu) == 0x03000011u);
    cpu.SetRegister(0, 0);
    REQUIRE(cpu.Step() == 10);
    REQUIRE(cpu.GetRegister(0) == 0x12345678u);
    REQUIRE(cpu.GetRegister(13) == 0x03000100u);
    REQUIRE(cpu.GetRegister(15) == 0x03000010u);
    REQUIRE((cpu.GetCpsr() & kThumbFlag) != 0);
}

TEST_CASE("Thumb multiple transfers handle overlap and empty list") {
    Bus bus(Rom({0xC003u, 0xC802u, 0xC000u})); Cpu cpu(bus); EnterThumb(cpu);
    cpu.SetRegister(0, 0x03000000u); cpu.SetRegister(1, 0xABCDEF01u);
    cpu.Step();
    REQUIRE(bus.Read32(0x03000000u) == 0x03000000u);
    REQUIRE(bus.Read32(0x03000004u) == 0xABCDEF01u);
    REQUIRE(cpu.GetRegister(0) == 0x03000008u);
    cpu.SetRegister(0, 0x03000004u);
    cpu.Step();
    REQUIRE(cpu.GetRegister(1) == 0xABCDEF01u);
    cpu.Step();
    REQUIRE(cpu.GetRegister(0) == 0x03000048u);
    REQUIRE(bus.Read32(0x03000008u) == 0x0800000Au);
}

TEST_CASE("Thumb STM stores updated base when base is not first") {
    // STMIA r1!,{r0,r1}: the later base entry sees writeback on ARM7TDMI.
    auto rom = Rom({0xC103u});
    Bus second_bus(rom); Cpu second(second_bus); EnterThumb(second);
    second.SetRegister(0, 0xAABBCCDDu);
    second.SetRegister(1, 0x03000020u);
    second.Step();
    REQUIRE(second_bus.Read32(0x03000020u) == 0xAABBCCDDu);
    REQUIRE(second_bus.Read32(0x03000024u) == 0x03000028u);
    REQUIRE(second.GetRegister(1) == 0x03000028u);
}

TEST_CASE("Thumb empty PUSH and POP transfer PC with 64 byte stack adjustment") {
    Bus bus(Rom({0xB400u, 0xBC00u})); Cpu cpu(bus); EnterThumb(cpu);
    cpu.SetRegister(13, 0x03000100u);
    cpu.Step();
    REQUIRE(cpu.GetRegister(13) == 0x030000C0u);
    REQUIRE(bus.Read32(0x030000C0u) == 0x08000006u);
    bus.Write32(0x030000C0u, 0x03000011u);
    cpu.Step();
    REQUIRE(cpu.GetRegister(13) == 0x03000100u);
    REQUIRE(cpu.GetRegister(15) == 0x03000010u);
}

TEST_CASE("Thumb branches test conditions, BL halves, and return address") {
    Bus bus(Rom({0xD001u, 0xD101u, 0xF000u, 0xF801u, 0xE7FEu}));
    Cpu cpu(bus); EnterThumb(cpu);
    REQUIRE(cpu.Step() == 5); // BEQ not taken
    REQUIRE(cpu.GetRegister(15) == 0x08000002u);
    REQUIRE(cpu.Step() == 11); // BNE taken to BL prefix at +6
    REQUIRE(cpu.GetRegister(15) == 0x08000008u);
}

TEST_CASE("Thumb BL combines signed high and low offsets") {
    Bus bus(Rom({0xF000u, 0xF801u, 0x46C0u, 0x46C0u})); Cpu cpu(bus); EnterThumb(cpu);
    REQUIRE(cpu.Step() == 5);
    REQUIRE(cpu.GetRegister(14) == 0x08000004u);
    REQUIRE(cpu.Step() == 11);
    REQUIRE(cpu.GetRegister(15) == 0x08000006u);
    REQUIRE(cpu.GetRegister(14) == 0x08000005u);
}

TEST_CASE("Thumb backward branches sign extend their displacement") {
    Bus bus(Rom({0xE7FEu})); Cpu cpu(bus); EnterThumb(cpu);
    REQUIRE(cpu.Step() == 13);
    REQUIRE(cpu.GetRegister(15) == 0x08000000u);

    Bus conditional_bus(Rom({0xD0FEu})); Cpu conditional(conditional_bus); EnterThumb(conditional, kZ);
    REQUIRE(conditional.Step() == 13);
    REQUIRE(conditional.GetRegister(15) == 0x08000000u);
}

TEST_CASE("Thumb BL negative displacement and interrupted prefix") {
    Bus bus(Rom({0xF7FFu, 0xF800u})); Cpu cpu(bus); EnterThumb(cpu);
    cpu.Step(); cpu.Step();
    REQUIRE(cpu.GetRegister(15) == 0x07FFF004u);
    REQUIRE(cpu.GetRegister(14) == 0x08000005u);

    Bus interrupted_bus(Rom({0xF000u, 0x2001u, 0xF800u}));
    Cpu interrupted(interrupted_bus); EnterThumb(interrupted);
    interrupted.Step(); interrupted.Step(); interrupted.Step();
    REQUIRE(interrupted.GetRegister(15) == 0x08000006u);
}

TEST_CASE("Thumb BX to ARM uses a word aligned target and ARM refill") {
    Bus bus(Rom({0x4708u})); Cpu cpu(bus); EnterThumb(cpu);
    cpu.SetRegister(1, 0x03000002u);
    REQUIRE(cpu.Step() == 7);
    REQUIRE(cpu.GetRegister(15) == 0x03000000u);
    REQUIRE((cpu.GetCpsr() & kThumbFlag) == 0);
}

TEST_CASE("Thumb LDM with base in list retains the loaded base") {
    Bus bus(Rom({0xC803u})); Cpu cpu(bus); EnterThumb(cpu);
    cpu.SetRegister(0, 0x03000000u);
    bus.Write32(0x03000000u, 0x11111111u);
    bus.Write32(0x03000004u, 0x22222222u);
    cpu.Step();
    REQUIRE(cpu.GetRegister(0) == 0x11111111u);
    REQUIRE(cpu.GetRegister(1) == 0x22222222u);
}

TEST_CASE("Thumb SWI and reserved opcode save following halfword return addresses") {
    for (auto [insn, mode, vector] : {std::tuple{0xDF42u, 0x13u, 8u},
                                     std::tuple{0xDE00u, 0x1Bu, 4u},
                                     std::tuple{0xE800u, 0x1Bu, 4u},
                                     std::tuple{0xBE00u, 0x1Bu, 4u}}) {
        Bus bus(Rom({static_cast<u16>(insn)})); Cpu cpu(bus); EnterThumb(cpu);
        REQUIRE(cpu.Step() == 7);
        REQUIRE((cpu.GetCpsr() & 31u) == mode);
        REQUIRE((cpu.GetCpsr() & kThumbFlag) == 0);
        REQUIRE(cpu.GetRegister(15) == vector);
        REQUIRE(cpu.GetRegister(14) == 0x08000002u);
    }
}

TEST_CASE("Thumb exceptions return to the following halfword in Thumb state") {
    for (auto [insn, vector] : {std::pair{0xDF00u, 8u},
                                std::pair{0xDE00u, 4u}}) {
        std::vector<u8> bios(16 * 1024);
        cpu_test::Put32(bios, vector, 0xE1B0F00Eu); // MOVS pc,lr
        Bus bus(Rom({static_cast<u16>(insn), 0x2055u}), bios);
        Cpu cpu(bus); EnterThumb(cpu, kC);
        cpu.Step();
        REQUIRE((cpu.GetCpsr() & kThumbFlag) == 0);
        cpu.Step();
        REQUIRE((cpu.GetCpsr() & kThumbFlag) != 0);
        REQUIRE((cpu.GetCpsr() & kC) != 0);
        REQUIRE(cpu.GetRegister(15) == 0x08000002u);
        cpu.Step();
        REQUIRE(cpu.GetRegister(0) == 0x55u);
    }
}
