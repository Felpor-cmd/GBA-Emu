#include <SDL2/SDL.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string_view>
#include <vector>

#include "cpu/cpu.hpp"
#include "memory/bus.hpp"
#include "ppu/ppu.hpp"

namespace {
std::vector<u8> LoadRom(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    const auto size = file.tellg();
    if (size <= 0) return {};
    file.seekg(0, std::ios::beg);
    std::vector<u8> data(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) return {};
    return data;
}

void FillTestColor(Bus& bus) {
    bus.Write16(0x04000000u, 0x0403u); // Mode 3, BG2 enabled.
    for (u32 pixel = 0; pixel < Ppu::kWidth * Ppu::kHeight; ++pixel)
        bus.Write16(0x06000000u + pixel * 2, 0x001Fu); // Red.
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4 ||
        (argc > 2 && (argc != 4 || std::string_view(argv[2]) != "--frames"))) {
        std::fprintf(stderr, "Usage: %s <rom.gba|--test-pattern> [--frames N]\n", argv[0]);
        return 1;
    }
    const bool test_pattern = std::string_view(argv[1]) == "--test-pattern";
    const long max_frames = argc == 4 ? std::strtol(argv[3], nullptr, 10) : 0;
    if (argc == 4 && max_frames <= 0) {
        std::fprintf(stderr, "Frame count must be positive.\n");
        return 1;
    }
    auto rom = test_pattern ? std::vector<u8>{} : LoadRom(argv[1]);
    if (!test_pattern && rom.empty()) {
        std::fprintf(stderr, "Could not load ROM: %s\n", argv[1]);
        return 1;
    }
    Bus bus(std::move(rom));
    Cpu cpu(bus);
    if (test_pattern) FillTestColor(bus);
    Ppu ppu(bus);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("GBA Emulator", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, Ppu::kWidth * 3, Ppu::kHeight * 3, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : nullptr;
    if (!renderer && window) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    SDL_Texture* texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, Ppu::kWidth, Ppu::kHeight) : nullptr;
    if (!window || !renderer || !texture) {
        std::fprintf(stderr, "SDL display: %s\n", SDL_GetError());
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, Ppu::kWidth, Ppu::kHeight);

    constexpr double kFrameSeconds = static_cast<double>(Ppu::kCyclesPerFrame) / 16777216.0;
    auto previous = std::chrono::steady_clock::now();
    double accumulator = 0.0;
    std::int64_t cycle_debt = 0;
    long frames = 0;
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) running = false;
        }
        const auto now = std::chrono::steady_clock::now();
        accumulator += std::chrono::duration<double>(now - previous).count();
        previous = now;
        if (accumulator > kFrameSeconds * 3) accumulator = kFrameSeconds * 3;
        if (accumulator < kFrameSeconds) {
            SDL_Delay(1);
            continue;
        }
        accumulator -= kFrameSeconds;
        if (!test_pattern) {
            cycle_debt += Ppu::kCyclesPerFrame;
            while (cycle_debt > 0) {
                const u32 elapsed = cpu.Step();
                ppu.Advance(elapsed);
                cycle_debt -= elapsed;
            }
        }
        ppu.RenderFrame();
        SDL_UpdateTexture(texture, nullptr, ppu.Pixels().data(), Ppu::kWidth * sizeof(u32));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        if (max_frames && ++frames >= max_frames) running = false;
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
