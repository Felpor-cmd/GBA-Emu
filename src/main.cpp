#include <cstdio>
#include <fstream>
#include <vector>

#include "cpu/cpu.hpp"
#include "memory/bus.hpp"

namespace {

std::vector<u8> LoadRom(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::fprintf(stderr, "Could not open ROM: %s\n", path);
        return {};
    }
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<u8> data(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        std::fprintf(stderr, "Failed to read ROM: %s\n", path);
        return {};
    }
    return data;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <rom.gba>\n", argv[0]);
        return 1;
    }

    std::vector<u8> rom = LoadRom(argv[1]);
    if (rom.empty()) {
        return 1;
    }
    std::printf("Loaded ROM: %s (%zu bytes)\n", argv[1], rom.size());

    Bus bus(std::move(rom));
    Cpu cpu(bus);
    cpu.Step();
    cpu.Reset();

    std::printf("CPU reset complete. Next: implement Cpu::Step().\n");
    return 0;
}
