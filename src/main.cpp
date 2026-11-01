#include <SDL2/SDL.h>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>
#include "emulator.hpp"
#include "input/input_manager.hpp"
#include "sound/audio_output.hpp"
#include "ui/input_settings_ui.hpp"
#include "memory/save_file.hpp"

namespace {
void AudioCallback(void* userdata, Uint8* stream, int length) {
    auto& output = *static_cast<AudioOutput*>(userdata);
    const unsigned frames = static_cast<unsigned>(length) / (sizeof(s16) * 2);
    output.Fill(reinterpret_cast<s16*>(stream), frames);
    const unsigned written = frames * 2 * sizeof(s16);
    if (written < static_cast<unsigned>(length))
        std::fill(stream + written, stream + length, 0);
}

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
    std::unique_ptr<SaveFile> save_file;
    bool save_loaded = false;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_AudioDeviceID audio_device = 0;
    std::unique_ptr<AudioOutput> audio_output;
    InputManager input;
    InputSettingsUi settings_ui;
    int result = 0;
    try {
        if (argc < 2) throw std::runtime_error("Usage: gba_emu <rom.gba|--test-pattern> [--headless] [--frames N] [--dump-frame file.ppm] [--save-path file.sav] [--save-type auto|sram|flash64|flash128|eeprom512|eeprom8k] [--no-save]");
        const bool pattern = std::string(argv[1]) == "--test-pattern";
        bool headless = false;
        bool trace = false;
        bool audio_diagnostics = false;
        bool no_save = false;
        BackupType save_type = BackupType::Auto;
        std::filesystem::path save_path;
        u64 limit = 0;
        std::string capture;
        for (int i = 2; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--headless") headless = true;
            else if (option == "--no-save") no_save = true;
            else if (option == "--save-path" && i + 1 < argc) save_path = argv[++i];
            else if (option == "--save-type" && i + 1 < argc) {
                const std::string type = argv[++i];
                if (type == "auto") save_type = BackupType::Auto;
                else if (type == "sram") save_type = BackupType::Sram;
                else if (type == "flash64") save_type = BackupType::Flash64;
                else if (type == "flash128") save_type = BackupType::Flash128;
                else if (type == "eeprom512") save_type = BackupType::Eeprom512;
                else if (type == "eeprom8k") save_type = BackupType::Eeprom8K;
                else throw std::runtime_error("Unknown save type: " + type);
            }
            else if (option == "--trace") trace = true;
            else if (option == "--audio-diagnostics") audio_diagnostics = true;
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
        if (pattern && (!save_path.empty() || save_type != BackupType::Auto))
            throw std::runtime_error("Save options require a ROM");
        if (no_save && !save_path.empty()) throw std::runtime_error("--no-save conflicts with --save-path");
        emulator = std::make_unique<Emulator>(pattern ? std::vector<u8>{} : LoadRom(argv[1]), !pattern);
        if (save_type != BackupType::Auto) emulator->Memory().ConfigureBackup(save_type);
        if (!pattern && !no_save) {
            if (save_path.empty()) save_path = std::filesystem::path(argv[1]).replace_extension(".sav");
            save_file = std::make_unique<SaveFile>(save_path);
            save_file->Load(emulator->Memory());
            save_loaded = true;
        }
        audio_output = std::make_unique<AudioOutput>(emulator->Memory());
        if (pattern) {
            auto& bus = emulator->Memory();
            bus.Write16(0x04000000u, 0x0403u);
            for (u32 pixel = 0; pixel < 240 * 160; ++pixel)
                bus.Write16(0x06000000u + pixel * 2, 31);
        }
        if (!headless) {
            SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0)
                throw std::runtime_error(SDL_GetError());
            window = SDL_CreateWindow("GBA Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                720, 480, SDL_WINDOW_SHOWN);
            if (window) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
            if (!renderer && window) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
            if (renderer) texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STREAMING, 240, 160);
            if (!texture) throw std::runtime_error(SDL_GetError());
            if (!input.Initialize()) throw std::runtime_error("Could not initialize controller input");
            if (!settings_ui.Initialize(window, renderer)) throw std::runtime_error("Could not initialize input settings UI");
            SDL_AudioSpec desired_audio{};
            desired_audio.freq = 48'000;
            desired_audio.format = AUDIO_S16SYS;
            desired_audio.channels = 2;
            desired_audio.samples = 1024;
            desired_audio.callback = AudioCallback;
            desired_audio.userdata = audio_output.get();
            audio_device = SDL_OpenAudioDevice(nullptr, 0, &desired_audio, nullptr, 0);
            if (!audio_device) std::cerr << "Audio output unavailable: " << SDL_GetError() << '\n';
        }
        constexpr double period = static_cast<double>(Ppu::kCyclesPerFrame) / 16777216.0;
        auto previous = std::chrono::steady_clock::now();
        const auto started = previous;
        auto next_save_flush = previous + std::chrono::seconds(5);
        auto next_audio_log = previous;
        std::ofstream audio_log;
        if (audio_diagnostics) {
            const auto directory = std::filesystem::temp_directory_path() / "gba-emu" /
                std::filesystem::path(argv[1]).stem() / "logs";
            std::filesystem::create_directories(directory);
            audio_log.open(directory / "playback.csv");
            if (!audio_log) throw std::runtime_error("Cannot open audio diagnostics log");
            audio_log << "wall_seconds,frames,backlog_seconds,queued,underrun_samples,dropped_samples,output_frames\n";
        }
        double accumulator = 0;
        bool audio_started = false;
        bool running = true;
        while (running) {
            if (save_file && std::chrono::steady_clock::now() >= next_save_flush) {
                save_file->Flush(emulator->Memory());
                next_save_flush = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            }
            if (!headless) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    settings_ui.ProcessEvent(event);
                    input.ProcessEvent(event);
                    if (event.type == SDL_QUIT) running = false;
                    if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.scancode == SDL_SCANCODE_F1)
                        settings_ui.ToggleMenu();
                    if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE &&
                        !input.IsRebinding() && !settings_ui.HasOpenPanel()) running = false;
                }
                if (!running) break;
                const auto now = std::chrono::steady_clock::now();
                accumulator += std::chrono::duration<double>(now - previous).count();
                previous = now;
                const bool keyboard_focused = SDL_GetKeyboardFocus() == window && !input.IsRebinding();
                emulator->Memory().SetKeyInput(input.PressedMask(keyboard_focused));
                unsigned catchup_frames = 0;
                bool frame_advanced = false;
                const auto frame_due = [&] {
                    return audio_started ? !audio_output->Ready() : accumulator >= period;
                };
                while (frame_due() && running && catchup_frames++ < 3) {
                    emulator->RunFrame(false);
                    frame_advanced = true;
                    // Once audio starts, its fixed-rate consumption is the
                    // clock. Replenish the queue instead of accumulating drift
                    // between independent video and audio clocks.
                    if (audio_started) accumulator = 0;
                    else accumulator -= period;
                    if (audio_device && !audio_started && audio_output->Ready()) {
                        SDL_PauseAudioDevice(audio_device, 0);
                        audio_started = true;
                    }
                    if (limit && emulator->Frames() >= limit) running = false;
                }
                if (renderer && frame_advanced && !frame_due()) {
                    emulator->RenderFrame();
                    SDL_UpdateTexture(texture, nullptr, emulator->Pixels().data(), 240 * sizeof(u32));
                    SDL_RenderClear(renderer);
                    int output_width = 0, output_height = 0;
                    SDL_GetRendererOutputSize(renderer, &output_width, &output_height);
                    const SDL_Rect destination{0, 0, output_width, output_height};
                    SDL_RenderCopy(renderer, texture, nullptr, &destination);
                    settings_ui.Render(input, window, renderer);
                }
                if (audio_diagnostics && now >= next_audio_log) {
                    audio_log << std::chrono::duration<double>(now - started).count() << ',' << emulator->Frames()
                        << ',' << accumulator << ',' << emulator->Memory().QueuedAudioSamples()
                        << ',' << audio_output->Underruns() << ',' << emulator->Memory().DroppedAudioSamples()
                        << ',' << audio_output->OutputFrames() << '\n';
                    next_audio_log = now + std::chrono::seconds(1);
                }
                if (!frame_due()) SDL_Delay(1);
            }
            if (headless) {
                emulator->RunFrame();
                if (limit && emulator->Frames() >= limit) running = false;
            }
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
    if (save_file && save_loaded && emulator) {
        try { save_file->Flush(emulator->Memory()); }
        catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            result = 1;
        }
    }
    settings_ui.Shutdown();
    input.Shutdown();
    if (audio_device) SDL_CloseAudioDevice(audio_device);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
