#include <array>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "cpu/cpu.hpp"

namespace {
constexpr u32 kRomBase = 0x08000000u;
constexpr u32 kInstructionLimit = 10'000'000u;
constexpr std::array<u32, 9> kEvalPrologue = {
    0xE92D0003u, 0xE3A00301u, 0xE5901004u, 0xE3110001u, 0x1AFFFFFCu,
    0xE5901004u, 0xE3110001u, 0x0AFFFFFCu, 0xE8BD0003u,
};

struct TraceEntry { u32 pc; u32 opcode; u32 cpsr; };

void PrintState(const Cpu& cpu, const std::array<TraceEntry, 16>& trace,
                u32 trace_count) {
    std::cerr << std::hex << std::setfill('0');
    for (u32 i = 0; i < 16; ++i) {
        std::cerr << "r" << std::dec << i << "=0x" << std::hex
                  << std::setw(8) << cpu.GetRegister(i) << ((i % 4 == 3) ? '\n' : ' ');
    }
    std::cerr << "cpsr=0x" << std::setw(8) << cpu.GetCpsr() << '\n';
    const u32 available = std::min(trace_count, 16u);
    for (u32 i = 0; i < available; ++i) {
        const auto& entry = trace[(trace_count - available + i) % trace.size()];
        std::cerr << "  pc=0x" << std::setw(8) << entry.pc
                  << " opcode=0x" << std::setw(8) << entry.opcode
                  << " cpsr=0x" << std::setw(8) << entry.cpsr << '\n';
    }
}

int Run(const std::string& path, u32 eval_address, u32 result_register,
        std::size_t expected_size) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open ROM: " + path);
    std::vector<u8> rom{std::istreambuf_iterator<char>(input),
                        std::istreambuf_iterator<char>()};
    if (rom.size() != expected_size || eval_address < kRomBase ||
        eval_address - kRomBase + kEvalPrologue.size() * 4 > rom.size() ||
        result_register >= 16) {
        throw std::runtime_error("ROM manifest does not match " + path);
    }
    Bus bus(std::move(rom));
    for (std::size_t i = 0; i < kEvalPrologue.size(); ++i) {
        if (bus.Read32(eval_address + static_cast<u32>(i * 4)) != kEvalPrologue[i])
            throw std::runtime_error("ROM eval signature changed: " + path);
    }

    Cpu cpu(bus);
    std::array<TraceEntry, 16> trace{};
    u32 trace_count = 0;
    for (u32 step = 0; step < kInstructionLimit; ++step) {
        const u32 pc = cpu.GetRegister(15);
        if (pc == eval_address) {
            const u32 result = cpu.GetRegister(static_cast<int>(result_register));
            if (result == 0) {
                std::cout << path << ": all opcode tests passed after " << step
                          << " instructions\n";
                return 0;
            }
            std::cerr << path << ": failed upstream test " << result
                      << " after " << step << " instructions\n";
            PrintState(cpu, trace, trace_count);
            return 1;
        }
        const u32 opcode = (cpu.GetCpsr() & kThumbFlag) ? bus.Read16(pc) : bus.Read32(pc);
        trace[trace_count++ % trace.size()] = {pc, opcode, cpu.GetCpsr()};
        cpu.Step();
    }
    std::cerr << path << ": did not reach eval within " << kInstructionLimit
              << " instructions\n";
    PrintState(cpu, trace, trace_count);
    return 2;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: opcode_rom_runner ROM EVAL_ADDRESS RESULT_REGISTER ROM_SIZE\n";
        return 2;
    }
    try {
        return Run(argv[1], static_cast<u32>(std::stoul(argv[2], nullptr, 0)),
                   static_cast<u32>(std::stoul(argv[3], nullptr, 0)),
                   std::stoul(argv[4], nullptr, 0));
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
