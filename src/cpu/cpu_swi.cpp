#include "cpu.hpp"

void Cpu::ExecuteSoftwareInterrupt(u32, u32 instruction_address) {
    EnterSoftwareInterruptException(instruction_address);
}
