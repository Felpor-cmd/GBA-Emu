#include <SDL2/SDL.h>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>
#include "emulator.hpp"

namespace {
std::vector<u8> LoadRom(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() <= 0) throw std::runtime_error("Cannot read ROM");
    std::vector<u8> data(static_cast<std::size_t>(file.tellg()));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(data.data()), data.size()))
        throw std::runtime_error("Cannot read ROM");
    return data;
}
}
int main(int argc, char** argv) {
    std::unique_ptr<Emulator> emulator;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    int result = 0;
    try {
        if (argc < 2) throw std::runtime_error("Usage: gba_emu <rom.gba|--test-pattern> [--headless] [--frames N] [--dump-frame file.ppm]");
        const bool pattern = std::string(argv[1]) == "--test-pattern";
        bool headless = false;
        bool trace = false;
        u64 limit = 0;
        std::string capture;
        for (int i = 2; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--headless") headless = true;
            else if (option == "--trace") trace = true;
            else if (option == "--frames" && i + 1 < argc) {
                const std::string value = argv[++i];
                std::size_t consumed = 0;
                if (value.empty() || value[0] == '-') throw std::runtime_error("Invalid frame count");
                limit = std::stoull(value, &consumed);
                if (!limit || consumed != value.size()) throw std::runtime_error("Invalid frame count");
            } else if (option == "--dump-frame" && i + 1 < argc) capture = argv[++i];
            else throw std::runtime_error("Unknown or incomplete option: " + option);
        }
        if (headless && !limit) throw std::runtime_error("--headless requires --frames N");
        emulator = std::make_unique<Emulator>(pattern ? std::vector<u8>{} : LoadRom(argv[1]), !pattern);
        if (pattern) {
            auto& bus = emulator->Memory();
            bus.Write16(0x04000000u, 0x0403u);
            for (u32 pixel = 0; pixel < 240 * 160; ++pixel)
                bus.Write16(0x06000000u + pixel * 2, 31);
        }
        if (!headless) {
            if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
            window = SDL_CreateWindow("GBA Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                720, 480, SDL_WINDOW_SHOWN);
            if (window) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
            if (!renderer && window) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
            if (renderer) texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STREAMING, 240, 160);
            if (!texture) throw std::runtime_error(SDL_GetError());
            SDL_RenderSetLogicalSize(renderer, 240, 160);
        }
        constexpr double period = static_cast<double>(Ppu::kCyclesPerFrame) / 16777216.0;
        auto previous = std::chrono::steady_clock::now();
        double accumulator = 0;
        bool running = true;
        while (running) {
            if (!headless) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) running = false;
                }
                if (!running) break;
                const auto now = std::chrono::steady_clock::now();
                accumulator = std::min(accumulator + std::chrono::duration<double>(now - previous).count(), period * 3);
                previous = now;
                if (accumulator < period) { SDL_Delay(1); continue; }
                accumulator -= period;
            }
            emulator->RunFrame();
            if (!headless) {
                SDL_UpdateTexture(texture, nullptr, emulator->Pixels().data(), 240 * sizeof(u32));
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                SDL_RenderPresent(renderer);
            }
            if (limit && emulator->Frames() >= limit) running = false;
        }
        if (!capture.empty()) emulator->DumpFrame(capture.c_str());
        if (trace) emulator->PrintTrace(std::cerr);
        if (headless) std::cout << "frames=" << emulator->Frames() << " pc=0x" << std::hex
            << emulator->Pc() << " dispcnt=0x" << emulator->Memory().Read16(0x04000000) << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        if (emulator) emulator->PrintTrace(std::cerr);
        result = 1;
    }
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
