#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdio>
#include <string>
#include <unistd.h>
#include <vector>

#include "cpu_test_helpers.hpp"

namespace {

constexpr u32 kControlField = 0x1;
constexpr u32 kFlagsAndControlFields = 0x9;
constexpr u32 kSupervisorMode = 0x13;

class StdoutCapture {
public:
    StdoutCapture() {
        REQUIRE(pipe(pipe_fds_) == 0);
        std::fflush(stdout);
        saved_stdout_ = dup(STDOUT_FILENO);
        REQUIRE(saved_stdout_ >= 0);
        REQUIRE(dup2(pipe_fds_[1], STDOUT_FILENO) >= 0);
        close(pipe_fds_[1]);
        pipe_fds_[1] = -1;
    }

    ~StdoutCapture() {
        std::fflush(stdout);
        if (saved_stdout_ >= 0) {
            dup2(saved_stdout_, STDOUT_FILENO);
            close(saved_stdout_);
        }
        close(pipe_fds_[0]);
    }

    std::string Read() {
        std::fflush(stdout);
        if (saved_stdout_ >= 0) {
            dup2(saved_stdout_, STDOUT_FILENO);
            close(saved_stdout_);
            saved_stdout_ = -1;
        }
        std::string output;
        char buffer[256];
        ssize_t bytes_read = 0;
        while ((bytes_read = read(pipe_fds_[0], buffer, sizeof(buffer))) > 0) {
            output.append(buffer, static_cast<std::size_t>(bytes_read));
        }
        return output;
    }

private:
    int pipe_fds_[2] = {-1, -1};
    int saved_stdout_ = -1;
};

u32 EncodeSwi(u32 cond, u32 comment) {
    return (cond << 28) | 0x0F000000u | (comment & 0x00FFFFFFu);
}

u32 EncodeMsrRegister(u32 cond, bool spsr, u32 field_mask, u32 rm) {
    return (cond << 28) | 0x0120F000u | (spsr ? 1u << 22 : 0) |
           ((field_mask & 0xF) << 16) | rm;
}

std::string RunSwi(u32 instruction) {
    Bus bus(cpu_test::AsRom(instruction));
    Cpu cpu(bus);
    StdoutCapture capture;

    cpu.Step();

    return capture.Read();
}

std::size_t CountOccurrences(const std::string& text, const std::string& value) {
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(value, position)) != std::string::npos) {
        ++count;
        position += value.size();
    }
    return count;
}

}  // namespace

TEST_CASE("SWI 0 requests BIOS call and advances the PC") {
    Bus bus(cpu_test::AsRom(0xEF000000u));
    Cpu cpu(bus);
    StdoutCapture capture;

    cpu.Step();

    std::string output = capture.Read();
    REQUIRE(CountOccurrences(output, "BIOS call requested") == 1);
    REQUIRE(output.find("comment=0x000000") != std::string::npos);
    REQUIRE(output.find("address=0x08000000") != std::string::npos);
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
}

TEST_CASE("SWI extracts the complete comment field") {
    std::string output = RunSwi(0xEF123456u);

    REQUIRE(CountOccurrences(output, "BIOS call requested") == 1);
    REQUIRE(output.find("comment=0x123456") != std::string::npos);
    REQUIRE(output.find("comment=0xF123456") == std::string::npos);
}

TEST_CASE("SWI exposes the GBA BIOS function number") {
    std::string output = RunSwi(0xEF060000u);

    REQUIRE(CountOccurrences(output, "BIOS call requested") == 1);
    REQUIRE(output.find("comment=0x060000") != std::string::npos);
    REQUIRE(output.find("function=0x06") != std::string::npos);
}

TEST_CASE("SWI accepts the maximum comment without sign extension") {
    std::string output = RunSwi(0xEFFFFFFFu);

    REQUIRE(CountOccurrences(output, "BIOS call requested") == 1);
    REQUIRE(output.find("comment=0xFFFFFF") != std::string::npos);
    REQUIRE(output.find("comment=0xFFFFFFFF") == std::string::npos);
}

TEST_CASE("SWIEQ requests BIOS call when Z is set") {
    std::vector<u8> rom(8, 0);
    cpu_test::Put32(rom, 0, cpu_test::Encode(0xE, 0xD, true, 0, 4, 4));
    cpu_test::Put32(rom, 4, EncodeSwi(0x0, 0x010000));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    u32 initial_cpsr = cpu.GetCpsr();
    StdoutCapture capture;

    cpu.Step();
    cpu.Step();

    std::string output = capture.Read();
    REQUIRE(CountOccurrences(output, "BIOS call requested") == 1);
    REQUIRE(output.find("comment=0x010000") != std::string::npos);
    REQUIRE(cpu.GetCpsr() == (initial_cpsr | 0x40000000u));
}

TEST_CASE("SWINE does nothing when Z is set") {
    std::vector<u8> rom(8, 0);
    cpu_test::Put32(rom, 0, cpu_test::Encode(0xE, 0xD, true, 0, 4, 4));
    cpu_test::Put32(rom, 4, EncodeSwi(0x1, 0x010000));

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(4, 0);
    cpu.Step();
    for (int reg = 0; reg < 15; ++reg) {
        cpu.SetRegister(reg, 0x10000000u + static_cast<u32>(reg));
    }
    u32 initial_cpsr = cpu.GetCpsr();
    StdoutCapture capture;

    std::array<u32, 15> registers_before_swi{};
    for (int reg = 0; reg < 15; ++reg) {
        registers_before_swi[reg] = cpu.GetRegister(reg);
    }
    cpu.Step();

    std::string output = capture.Read();
    REQUIRE(CountOccurrences(output, "BIOS call requested") == 0);
    for (int reg = 0; reg < 15; ++reg) {
        REQUIRE(cpu.GetRegister(reg) == registers_before_swi[reg]);
    }
    REQUIRE(cpu.GetCpsr() == initial_cpsr);
    REQUIRE(cpu.GetRegister(15) == 0x08000008u);
}

TEST_CASE("SWI preserves general-purpose registers") {
    Bus bus(cpu_test::AsRom(0xEF000000u));
    Cpu cpu(bus);
    std::array<u32, 15> registers_before_swi{};
    for (int reg = 0; reg < 15; ++reg) {
        registers_before_swi[reg] = 0xABC00000u + static_cast<u32>(reg);
        cpu.SetRegister(reg, registers_before_swi[reg]);
    }
    StdoutCapture capture;

    cpu.Step();

    capture.Read();
    for (int reg = 0; reg < 15; ++reg) {
        REQUIRE(cpu.GetRegister(reg) == registers_before_swi[reg]);
    }
}

TEST_CASE("SWI preserves CPSR and SPSR_svc") {
    std::vector<u8> rom(16, 0);
    cpu_test::Put32(rom, 0, EncodeMsrRegister(0xE, false, kControlField, 0));
    cpu_test::Put32(rom, 4, EncodeMsrRegister(
        0xE, true, kFlagsAndControlFields, 1));
    cpu_test::Put32(rom, 8, 0xEF000000u);
    cpu_test::Put32(rom, 12, 0xE14F3000u);  // MRS r3, SPSR

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.SetRegister(0, kSupervisorMode);
    cpu.SetRegister(1, 0xA00000D3u);
    cpu.Step();
    cpu.Step();
    u32 cpsr_before_swi = cpu.GetCpsr();
    cpu.SetRegister(2, 0xDEADBEEFu);
    StdoutCapture capture;

    cpu.Step();

    REQUIRE(cpu.GetCpsr() == cpsr_before_swi);
    cpu.Step();

    capture.Read();
    REQUIRE(cpu.GetCpsr() == cpsr_before_swi);
    REQUIRE((cpu.GetCpsr() & 0x1Fu) == kSupervisorMode);
    REQUIRE((cpu.GetCpsr() & kThumbFlag) == 0);
    REQUIRE(cpu.GetRegister(3) == 0xA00000D3u);
    REQUIRE(cpu.GetRegister(2) == 0xDEADBEEFu);
}

TEST_CASE("SWI stub does not jump to the exception vector") {
    Bus bus(cpu_test::AsRom(0xEF000000u));
    Cpu cpu(bus);
    StdoutCapture capture;

    cpu.Step();

    capture.Read();
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
    REQUIRE(cpu.GetRegister(15) != 0x00000008u);
}

TEST_CASE("SWI does not modify memory or push the stack") {
    Bus bus(cpu_test::AsRom(0xEF000000u));
    constexpr u32 kMemoryAddress = 0x02000000u;
    constexpr u32 kMemoryValue = 0xCAFEBABEu;
    bus.Write32(kMemoryAddress, kMemoryValue);
    Cpu cpu(bus);
    u32 initial_sp = cpu.GetRegister(13);
    StdoutCapture capture;

    cpu.Step();

    capture.Read();
    REQUIRE(bus.Read32(kMemoryAddress) == kMemoryValue);
    REQUIRE(cpu.GetRegister(13) == initial_sp);
}

TEST_CASE("SWI encoding reaches the SWI handler") {
    std::string output = RunSwi(0xEF123456u);

    REQUIRE(output.find("BIOS call requested") != std::string::npos);
    REQUIRE(output.find("unknown instruction") == std::string::npos);
    REQUIRE(output.find("branch") == std::string::npos);
    REQUIRE(output.find("coprocessor") == std::string::npos);
}

TEST_CASE("Consecutive SWIs produce separate BIOS requests") {
    std::vector<u8> rom(8, 0);
    cpu_test::Put32(rom, 0, 0xEF010203u);
    cpu_test::Put32(rom, 4, 0xEF0A0B0Cu);

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    StdoutCapture capture;

    cpu.Step();
    REQUIRE(cpu.GetRegister(15) == 0x08000004u);
    cpu.Step();

    std::string output = capture.Read();
    REQUIRE(CountOccurrences(output, "BIOS call requested") == 2);
    REQUIRE(output.find("comment=0x010203") != std::string::npos);
    REQUIRE(output.find("comment=0x0A0B0C") != std::string::npos);
    REQUIRE(output.find("address=0x08000000") != std::string::npos);
    REQUIRE(output.find("address=0x08000004") != std::string::npos);
    REQUIRE(cpu.GetRegister(15) == 0x08000008u);
}
