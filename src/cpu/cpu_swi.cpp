#include "cpu.hpp"

void Cpu::ExecuteSoftwareInterrupt(u32 instruction, u32 instruction_address) {
    const bool thumb = (cpsr_ & kThumbFlag) != 0;
    const u32 swi = thumb ? instruction & 0xFFu : instruction & 0x00FFFFFFu;
    if (swi == 1) { // RegisterRamReset
        bus_.ResetMemory(regs_[0]);
        regs_[15] = instruction_address + (thumb ? 2u : 4u);
        next_fetch_sequential_ = false;
        return;
    }
    EnterSoftwareInterruptException(instruction_address);
}
