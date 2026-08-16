#pragma once
#include <cstdint>

// The GBA is a 32-bit little-endian machine. These short aliases match the
// naming convention used in GBATEK and most GBA emulator source, so it's
// easier to cross-reference the docs while you code.
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using s8  = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;
