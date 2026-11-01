#pragma once

#include "../types.hpp"

namespace cpu_detail {

inline u32 RotateRight(u32 value, unsigned amount) {
    amount &= 31;
    if (amount == 0) {
        return value;
    }
    return (value >> amount) | (value << (32 - amount));
}

}  // namespace cpu_detail
