#pragma once

#include <cstddef>
#include <vector>

#include "cpu/cpu.hpp"

namespace cpu_test {

inline void Put32(std::vector<u8>& rom, std::size_t offset, u32 value) {
    rom[offset + 0] = static_cast<u8>(value & 0xFF);
    rom[offset + 1] = static_cast<u8>((value >> 8) & 0xFF);
    rom[offset + 2] = static_cast<u8>((value >> 16) & 0xFF);
    rom[offset + 3] = static_cast<u8>((value >> 24) & 0xFF);
}

inline u32 Encode(u32 cond, u32 opcode, bool s, u32 rn, u32 rd, u32 rm) {
    return (cond << 28) | (0b00 << 26) | (opcode << 21) |
           ((s ? 1u : 0u) << 20) | (rn << 16) | (rd << 12) | rm;
}

inline u32 EncodeShiftedRegister(u32 cond, u32 opcode, bool s, u32 rn, u32 rd,
                                 u32 shift_amount, u32 shift_type, u32 rm) {
    return (cond << 28) | (opcode << 21) | ((s ? 1u : 0u) << 20) |
           (rn << 16) | (rd << 12) | (shift_amount << 7) |
           (shift_type << 5) | rm;
}

inline u32 EncodeSingleDataTransfer(u32 cond, bool load, bool byte, u32 rn, u32 rd,
                                    s32 offset, bool pre_index = true,
                                    bool writeback = false) {
    bool add_offset = offset >= 0;
    u32 magnitude = static_cast<u32>(add_offset ? offset : -offset);
    return (cond << 28) | (0b01 << 26) |
           (pre_index ? 1u : 0u) << 24 |
           (add_offset ? 1u : 0u) << 23 |
           (byte ? 1u : 0u) << 22 |
           (writeback ? 1u : 0u) << 21 |
           (load ? 1u : 0u) << 20 |
           (rn << 16) | (rd << 12) | (magnitude & 0xFFF);
}

inline u32 EncodeRegisterDataTransfer(u32 cond, bool load, bool byte, u32 rn, u32 rd,
                                      u32 rm, u32 shift_amount = 0,
                                      u32 shift_type = 0, bool pre_index = true,
                                      bool add_offset = true, bool writeback = false) {
    return (cond << 28) | (0b01 << 26) | (1u << 25) |
           (pre_index ? 1u : 0u) << 24 |
           (add_offset ? 1u : 0u) << 23 |
           (byte ? 1u : 0u) << 22 |
           (writeback ? 1u : 0u) << 21 |
           (load ? 1u : 0u) << 20 |
           (rn << 16) | (rd << 12) | (shift_amount << 7) |
           (shift_type << 5) | (rm & 0xF);
}

inline u32 EncodeHalfwordDataTransfer(u32 cond, bool load, bool signed_transfer,
                                      bool halfword, u32 rn, u32 rd, s32 offset,
                                      bool pre_index = true, bool writeback = false) {
    bool add_offset = offset >= 0;
    u32 magnitude = static_cast<u32>(add_offset ? offset : -offset);
    return (cond << 28) | (pre_index ? 1u : 0u) << 24 |
           (add_offset ? 1u : 0u) << 23 | (1u << 22) |
           (writeback ? 1u : 0u) << 21 | (load ? 1u : 0u) << 20 |
           (rn << 16) | (rd << 12) | (((magnitude >> 4) & 0xF) << 8) |
           (1u << 7) | (signed_transfer ? 1u : 0u) << 6 |
           (halfword ? 1u : 0u) << 5 | (1u << 4) | (magnitude & 0xF);
}

inline u32 EncodeBlockDataTransfer(u32 cond, bool load, u32 rn, u16 register_list,
                                   bool pre_index, bool add_offset,
                                   bool writeback = false, bool set_status = false) {
    return (cond << 28) | (0b100 << 25) |
           (pre_index ? 1u : 0u) << 24 |
           (add_offset ? 1u : 0u) << 23 |
           (set_status ? 1u : 0u) << 22 |
           (writeback ? 1u : 0u) << 21 |
           (load ? 1u : 0u) << 20 |
           (rn << 16) | register_list;
}

inline std::vector<u8> AsRom(u32 instr) {
    return {static_cast<u8>(instr), static_cast<u8>(instr >> 8),
            static_cast<u8>(instr >> 16), static_cast<u8>(instr >> 24)};
}

}  // namespace cpu_test
