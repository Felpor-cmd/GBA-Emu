#include <mgba/flags.h>
#include <mgba/core/core.h>
#include <mgba/core/version.h>
#include <mgba-util/vfs.h>

#include <fcntl.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "cpu/cpu.hpp"

namespace {
u32 ReferenceRegister(mCore* core, const std::string& name) {
    u32 value = 0;
    if (!core->readRegister(core, name.c_str(), &value)) {
        throw std::runtime_error("mGBA could not read " + name);
    }
    return value;
}

void CompareValue(const std::string& fixture, unsigned step, const std::string& name,
                  u32 actual, u32 reference) {
    if (actual != reference) {
        std::cerr << fixture << " instruction " << step << ": " << name
                  << " ours=0x" << std::hex << actual << " mGBA=0x" << reference << '\n';
        throw std::runtime_error("first trace mismatch");
    }
}

void CompareFixture(const std::string& path, unsigned instructions) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open " + path);
    std::vector<u8> rom{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    Bus bus(rom);
    Cpu cpu(bus);
    cpu.SetRegister(2, 0x03000000u);

    mCore* reference = mCoreCreate(mPLATFORM_GBA);
    if (std::string(projectVersion) != "0.10.5")
        throw std::runtime_error("reference mGBA must be version 0.10.5");
    if (!reference || !reference->init(reference)) throw std::runtime_error("mGBA initialization failed");
    mCoreInitConfig(reference, nullptr);
    VFile* rom_file = VFileOpen(path.c_str(), O_RDONLY);
    if (!rom_file || !reference->loadROM(reference, rom_file))
        throw std::runtime_error("mGBA could not load " + path);
    reference->opts.skipBios = true;
    reference->reset(reference);
    const u32 cpsr = cpu.GetCpsr();
    if (!reference->writeRegister(reference, "cpsr", &cpsr))
        throw std::runtime_error("mGBA could not set CPSR");
    for (int reg = 0; reg < 15; ++reg) {
        const u32 value = cpu.GetRegister(reg);
        if (!reference->writeRegister(reference, ("r" + std::to_string(reg)).c_str(), &value))
            throw std::runtime_error("mGBA could not set register");
    }
    const u32 pc = cpu.GetRegister(15);
    if (!reference->writeRegister(reference, "pc", &pc))
        throw std::runtime_error("mGBA could not set PC");

    for (unsigned step = 1; step <= instructions; ++step) {
        cpu.Step();
        reference->step(reference);
        for (int reg = 0; reg < 15; ++reg) {
            const auto name = "r" + std::to_string(reg);
            CompareValue(path, step, name, cpu.GetRegister(reg), ReferenceRegister(reference, name));
        }
        CompareValue(path, step, "cpsr", cpu.GetCpsr(), ReferenceRegister(reference, "cpsr"));
        // mGBA exposes the pipeline PC ahead of the next instruction by one
        // instruction width in either state.
        const u32 pipeline_width = (cpu.GetCpsr() & kThumbFlag) ? 2u : 4u;
        CompareValue(path, step, "pc", cpu.GetRegister(15),
                     ReferenceRegister(reference, "pc") - pipeline_width);
        for (u32 address : {0x03000000u, 0x03000004u, 0x03000008u,
                            0x03000010u, 0x03000014u, 0x03000018u,
                            0x03007EF4u, 0x03007EF8u, 0x03007EFCu}) {
            CompareValue(path, step, "[" + std::to_string(address) + "]",
                         bus.Read32(address), reference->busRead32(reference, address));
        }
    }
    reference->deinit(reference);
    std::cout << path << ": " << instructions << " instructions matched mGBA 0.10.5\n";
}
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: mgba_trace_compare ROM STEPS\n";
        return 2;
    }
    try {
        CompareFixture(argv[1], static_cast<unsigned>(std::stoul(argv[2])));
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
