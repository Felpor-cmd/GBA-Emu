#include "ppu.hpp"

#include <algorithm>
#include <optional>

namespace {
constexpr u32 kDispcnt = 0x04000000u;
constexpr u32 kPalette = 0x05000000u;
constexpr u32 kVram = 0x06000000u;
constexpr u32 kOam = 0x07000000u;

u16 ApplyEffect(u16 front, u16 back, unsigned mode, unsigned a, unsigned b) {
    u16 result = 0;
    for (unsigned shift : {0u, 5u, 10u}) {
        const unsigned first = (front >> shift) & 31u, second = (back >> shift) & 31u;
        unsigned value = first;
        if (mode == 1) value = std::min(31u, (first * a + second * b) >> 4);
        else if (mode == 2) value = first + ((31u - first) * a >> 4);
        else if (mode == 3) value = first - (first * a >> 4);
        result |= static_cast<u16>(value << shift);
    }
    return result;
}

bool WindowContains(u16 bounds, int coordinate) {
    const int start = bounds >> 8, end = bounds & 255;
    return start <= end ? coordinate >= start && coordinate < end
                        : coordinate >= start || coordinate < end;
}

s32 SignedReference(u32 value) {
    value &= 0x0FFFFFFFu;
    if (value & 0x08000000u) value |= 0xF0000000u;
    return static_cast<s32>(value);
}

std::optional<u16> TextBackgroundPixel(const Bus& bus, u32 bg, int x, int y) {
    const u16 control = bus.Read16(0x04000008u + bg * 2);
    const u32 size = control >> 14;
    const int width = (size & 1u) ? 512 : 256;
    const int height = (size & 2u) ? 512 : 256;
    const u32 scroll_base = 0x04000010u + bg * 4;
    const int px = (x + (bus.Read16(scroll_base) & 511)) % width;
    const int py = (y + (bus.Read16(scroll_base + 2) & 511)) % height;
    const int tx = px / 8, ty = py / 8;
    const u32 block = static_cast<u32>((ty / 32) * (width / 256) + tx / 32);
    const u32 map = kVram + ((control >> 8) & 31u) * 0x800u + block * 0x800u;
    const u16 entry = bus.Read16(map + static_cast<u32>((ty % 32) * 32 + tx % 32) * 2);
    const u32 tile = entry & 1023u;
    const int sx = (entry & (1u << 10)) ? 7 - px % 8 : px % 8;
    const int sy = (entry & (1u << 11)) ? 7 - py % 8 : py % 8;
    const u32 chars = kVram + ((control >> 2) & 3u) * 0x4000u;
    u32 index;
    if (control & (1u << 7)) {
        index = bus.Read8(chars + tile * 64 + sy * 8 + sx);
    } else {
        const u8 packed = bus.Read8(chars + tile * 32 + sy * 4 + sx / 2);
        index = (packed >> ((sx & 1) * 4)) & 15u;
        index += ((entry >> 12) & 15u) * 16u;
    }
    if ((index & ((control & (1u << 7)) ? 255u : 15u)) == 0) return std::nullopt;
    return bus.Read16(kPalette + index * 2);
}

std::optional<u16> AffineBackgroundPixel(const Bus& bus, u32 bg, int x, int y) {
    const u16 control = bus.Read16(0x04000008u + bg * 2);
    const u32 affine = 0x04000020u + (bg - 2) * 0x10u;
    const s32 pa = static_cast<s16>(bus.Read16(affine));
    const s32 pb = static_cast<s16>(bus.Read16(affine + 2));
    const s32 pc = static_cast<s16>(bus.Read16(affine + 4));
    const s32 pd = static_cast<s16>(bus.Read16(affine + 6));
    const s64 fx = static_cast<s64>(SignedReference(bus.Read32(affine + 8))) +
                   static_cast<s64>(pa) * x + static_cast<s64>(pb) * y;
    const s64 fy = static_cast<s64>(SignedReference(bus.Read32(affine + 12))) +
                   static_cast<s64>(pc) * x + static_cast<s64>(pd) * y;
    const int size = 128 << (control >> 14);
    int px = static_cast<int>(fx >> 8), py = static_cast<int>(fy >> 8);
    if (control & (1u << 13)) {
        px = (px % size + size) % size;
        py = (py % size + size) % size;
    } else if (px < 0 || py < 0 || px >= size || py >= size) {
        return std::nullopt;
    }
    const u32 map = kVram + ((control >> 8) & 31u) * 0x800u;
    const u32 chars = kVram + ((control >> 2) & 3u) * 0x4000u;
    const u32 tile = bus.Read8(map + static_cast<u32>((py / 8) * (size / 8) + px / 8));
    const u32 index = bus.Read8(chars + tile * 64 + (py % 8) * 8 + px % 8);
    if (!index) return std::nullopt;
    return bus.Read16(kPalette + index * 2);
}

std::optional<u16> BitmapPixel(const Bus& bus, u32 mode, u16 dispcnt, int x, int y) {
    const u32 page = (mode >= 4 && (dispcnt & (1u << 4))) ? 0xA000u : 0u;
    const int width = mode == 5 ? 160 : 240;
    const int height = mode == 5 ? 128 : 160;
    if (x >= width || y >= height) return std::nullopt;
    const u32 offset = static_cast<u32>(y * width + x);
    if (mode == 4) {
        const u8 index = bus.Read8(kVram + page + offset);
        if (!index) return std::nullopt;
        return bus.Read16(kPalette + static_cast<u32>(index) * 2);
    }
    return bus.Read16(kVram + page + offset * 2);
}

struct ObjectSize { int width; int height; };
constexpr ObjectSize kObjectSizes[3][4] = {
    {{8,8}, {16,16}, {32,32}, {64,64}},
    {{16,8}, {32,8}, {32,16}, {64,32}},
    {{8,16}, {8,32}, {16,32}, {32,64}},
};

std::optional<u16> ObjectPixel(const Bus& bus, u16 dispcnt, u32 object,
                               int screen_x, int screen_y) {
    const u32 address = kOam + object * 8;
    const u16 attr0 = bus.Read16(address);
    const u16 attr1 = bus.Read16(address + 2);
    const u16 attr2 = bus.Read16(address + 4);
    const bool affine = (attr0 & (1u << 8)) != 0;
    if (!affine && (attr0 & (1u << 9))) return std::nullopt;
    const u32 shape = attr0 >> 14;
    if (shape >= 3) return std::nullopt;
    const auto size = kObjectSizes[shape][attr1 >> 14];
    const int box_width = size.width * (affine && (attr0 & (1u << 9)) ? 2 : 1);
    const int box_height = size.height * (affine && (attr0 & (1u << 9)) ? 2 : 1);
    int ox = attr1 & 511u, oy = attr0 & 255u;
    if (ox >= 256) ox -= 512;
    if (oy >= 160) oy -= 256;
    int sx = screen_x - ox, sy = screen_y - oy;
    if (sx < 0 || sy < 0 || sx >= box_width || sy >= box_height) return std::nullopt;
    if (affine) {
        const u32 parameters = (attr1 >> 9) & 31u;
        const u32 matrix = kOam + parameters * 32 + 6;
        const s32 pa = static_cast<s16>(bus.Read16(matrix));
        const s32 pb = static_cast<s16>(bus.Read16(matrix + 8));
        const s32 pc = static_cast<s16>(bus.Read16(matrix + 16));
        const s32 pd = static_cast<s16>(bus.Read16(matrix + 24));
        const int dx = sx - box_width / 2, dy = sy - box_height / 2;
        sx = ((pa * dx + pb * dy) >> 8) + size.width / 2;
        sy = ((pc * dx + pd * dy) >> 8) + size.height / 2;
        if (sx < 0 || sy < 0 || sx >= size.width || sy >= size.height) return std::nullopt;
    } else {
        if (attr1 & (1u << 12)) sx = size.width - 1 - sx;
        if (attr1 & (1u << 13)) sy = size.height - 1 - sy;
    }
    const bool color_256 = (attr0 & (1u << 13)) != 0;
    const u32 tile_base = (color_256 && !(dispcnt & (1u << 6)))
        ? (attr2 & 1022u) : (attr2 & 1023u);
    if ((dispcnt & 7u) >= 3 && tile_base < 512) return std::nullopt;
    const u32 tile_step = color_256 ? 2u : 1u;
    const u32 row_step = (dispcnt & (1u << 6))
        ? static_cast<u32>(size.width / 8) * tile_step : 32u;
    const u32 tile = (tile_base + static_cast<u32>(sy / 8) * row_step +
                      static_cast<u32>(sx / 8) * tile_step) & 1023u;
    const u32 address_in_tile = static_cast<u32>((sy % 8) * (color_256 ? 8 : 4));
    u32 index;
    if (color_256) {
        index = bus.Read8(kVram + 0x10000u + tile * 32 + address_in_tile + sx % 8);
    } else {
        const u8 packed = bus.Read8(kVram + 0x10000u + tile * 32 +
                                    address_in_tile + sx % 8 / 2);
        index = (packed >> ((sx & 1) * 4)) & 15u;
    }
    if (!index) return std::nullopt;
    const u32 palette_index = color_256 ? index : ((attr2 >> 12) * 16u + index);
    return bus.Read16(kPalette + 0x200u + palette_index * 2);
}
}

u32 Ppu::ToRgba(u16 color) {
    const u32 red = color & 31u;
    const u32 green = (color >> 5) & 31u;
    const u32 blue = (color >> 10) & 31u;
    return 0xFF000000u | ((red << 3 | red >> 2) << 16) |
           ((green << 3 | green >> 2) << 8) | (blue << 3 | blue >> 2);
}

u32 Ppu::Advance(u32 cycles) {
    u32 events = 0;
    const u32 old_cycles = line_cycles_;
    line_cycles_ += cycles;
    if (old_cycles < 1006 && line_cycles_ >= 1006) {
        if (scanline_ < 160) events |= 2;
        if (bus_.Read16(0x04000004u) & (1u << 4)) bus_.RequestInterrupt(2);
    }
    while (line_cycles_ >= 1232) {
        line_cycles_ -= 1232;
        scanline_ = static_cast<u16>((scanline_ + 1) % 228);
        const u16 status = bus_.Read16(0x04000004u);
        if (scanline_ == 160) {
            events |= 1;
            if (status & (1u << 3)) bus_.RequestInterrupt(1);
        }
        if (scanline_ == (status >> 8) && (status & (1u << 5))) bus_.RequestInterrupt(4);
    }
    bus_.SetVideoTiming(scanline_, line_cycles_ >= 1006);
    return events;
}

void Ppu::RenderFrame() {
    const u16 dispcnt = bus_.Read16(kDispcnt);
    const u32 mode = dispcnt & 7u;
    const u32 backdrop = ToRgba(bus_.Read16(kPalette));
    if (dispcnt & (1u << 7)) {
        pixels_.fill(0xFFFFFFFFu);
        return;
    }
    pixels_.fill(backdrop);
    std::array<u32, kWidth * kHeight> object_keys;
    std::array<u16, kWidth * kHeight> object_colors{};
    std::array<bool, kWidth * kHeight> object_semi{};
    std::array<bool, kWidth * kHeight> object_window{};
    object_keys.fill(0xFFFFFFFFu);
    if (dispcnt & (1u << 12)) {
        for (u32 object = 0; object < 128; ++object) {
            const u32 address = kOam + object * 8;
            const u16 attr0 = bus_.Read16(address);
            const u16 attr1 = bus_.Read16(address + 2);
            const u16 attr2 = bus_.Read16(address + 4);
            const u32 shape = attr0 >> 14;
            const u32 object_mode = (attr0 >> 10) & 3u;
            if (shape >= 3 || (!(attr0 & (1u << 8)) && (attr0 & (1u << 9))) ||
                object_mode == 3 || (object_mode == 2 && !(dispcnt & (1u << 15)))) continue;
            const auto size = kObjectSizes[shape][attr1 >> 14];
            const bool double_size = (attr0 & 0x0300u) == 0x0300u;
            const int width = size.width * (double_size ? 2 : 1);
            const int height = size.height * (double_size ? 2 : 1);
            int ox = attr1 & 511u, oy = attr0 & 255u;
            if (ox >= 256) ox -= 512;
            if (oy >= 160) oy -= 256;
            const u32 key = ((attr2 >> 10) & 3u) * 256u;
            for (int y = std::max(0, oy); y < std::min(kHeight, oy + height); ++y) {
                for (int x = std::max(0, ox); x < std::min(kWidth, ox + width); ++x) {
                    const u32 offset = static_cast<u32>(y * kWidth + x);
                    if (object_mode == 2) {
                        if (!object_window[offset] && ObjectPixel(bus_, dispcnt, object, x, y))
                            object_window[offset] = true;
                        continue;
                    }
                    if (object_keys[offset] != 0xFFFFFFFFu) continue;
                    const auto color = ObjectPixel(bus_, dispcnt, object, x, y);
                    if (color) {
                        object_keys[offset] = key;
                        object_colors[offset] = *color;
                        object_semi[offset] = ((attr0 >> 10) & 3u) == 1;
                    }
                }
            }
        }
    }
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            u16 mask = 0x3Fu;
            if (dispcnt & 0xE000u) {
                const u16 winout = bus_.Read16(0x0400004Au);
                const u32 offset = static_cast<u32>(y * kWidth + x);
                mask = object_window[offset] ? (winout >> 8) & 0x3Fu : winout & 0x3Fu;
                for (int win = 1; win >= 0; --win) {
                    if (!(dispcnt & (1u << (13 + win)))) continue;
                    if (WindowContains(bus_.Read16(0x04000040u + win * 2), x) &&
                        WindowContains(bus_.Read16(0x04000044u + win * 2), y))
                        mask = (bus_.Read16(0x04000048u) >> (win * 8)) & 0x3Fu;
                }
            }
            u32 best_key = 0xFFFFFFFEu, second_key = 0xFFFFFFFFu;
            u16 best_color = bus_.Read16(kPalette), second_color = 0;
            unsigned best_layer = 5, second_layer = 6;
            auto add = [&](u32 key, u16 color, unsigned layer) {
                if (key < best_key) {
                    second_key = best_key; second_color = best_color; second_layer = best_layer;
                    best_key = key; best_color = color; best_layer = layer;
                } else if (key < second_key) {
                    second_key = key; second_color = color; second_layer = layer;
                }
            };
            for (u32 bg = 0; bg < 4; ++bg) {
                if (!(dispcnt & (1u << (8 + bg))) || !(mask & (1u << bg))) continue;
                std::optional<u16> color;
                if (mode == 0 || (mode == 1 && bg < 2))
                    color = TextBackgroundPixel(bus_, bg, x, y);
                else if ((mode == 1 && bg == 2) || (mode == 2 && bg >= 2))
                    color = AffineBackgroundPixel(bus_, bg, x, y);
                else if (mode >= 3 && mode <= 5 && bg == 2)
                    color = BitmapPixel(bus_, mode, dispcnt, x, y);
                if (!color) continue;
                const u32 priority = mode >= 3 ? (bus_.Read16(0x0400000Cu) & 3u)
                    : (bus_.Read16(0x04000008u + bg * 2) & 3u);
                const u32 key = priority * 256 + bg + 128;
                add(key, *color, bg);
            }
            const u32 offset = static_cast<u32>(y * kWidth + x);
            if ((mask & 16u) && object_keys[offset] != 0xFFFFFFFFu)
                add(object_keys[offset], object_colors[offset], 4);
            const u16 blend = bus_.Read16(0x04000050u);
            const bool semi = best_layer == 4 && object_semi[offset];
            const unsigned mode_effect = (blend >> 6) & 3u;
            const bool first_target = (blend & (1u << best_layer)) != 0;
            const bool second_target = second_layer < 6 && (blend & (1u << (8 + second_layer)));
            if (mask & 32u) {
                if ((semi || (mode_effect == 1 && first_target)) && second_target) {
                    const u16 alpha = bus_.Read16(0x04000052u);
                    best_color = ApplyEffect(best_color, second_color, 1,
                        std::min(16u, alpha & 31u), std::min(16u, (alpha >> 8) & 31u));
                } else if (first_target && mode_effect >= 2) {
                    best_color = ApplyEffect(best_color, 0, mode_effect,
                        std::min(16u, bus_.Read16(0x04000054u) & 31u), 0);
                }
            }
            pixels_[y * kWidth + x] = ToRgba(best_color);
        }
    }
}
