#include "cpu.hpp"

#include <cstdio>

void Cpu::ExecuteSoftwareInterrupt(u32 instruction, u32 instruction_address) {
    u32 comment = instruction & 0x00FFFFFFu;
    u32 bios_function = (comment >> 16) & 0xFFu;

    std::printf("BIOS call requested: comment=0x%06X function=0x%02X "
                "address=0x%08X\n",
                static_cast<unsigned int>(comment),
                static_cast<unsigned int>(bios_function),
                static_cast<unsigned int>(instruction_address));
}
