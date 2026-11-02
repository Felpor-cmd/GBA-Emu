#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include "emulator.hpp"
#include "sound/audio_output.hpp"
#ifdef HAVE_MGBA
#include <mgba/flags.h>
#include <mgba/core/core.h>
#include <mgba/core/version.h>
#include <mgba/core/blip_buf.h>
#include <mgba-util/vfs.h>
#include <mgba/internal/gba/gba.h>
#include <fcntl.h>
#endif

namespace {
#ifdef HAVE_MGBA
using TimingCallback = void (*)(mTiming*, void*, uint32_t);
std::array<TimingCallback, 4> original_timers{};
TimingCallback original_dma = nullptr;
std::ofstream* reference_events = nullptr;
template<unsigned Index>
void ReferenceTimer(mTiming* timing, void* context, uint32_t late) {
    auto* gba = static_cast<GBA*>(context);
    const u64 cycle = static_cast<u32>(mTimingCurrentTime(timing) - late);
    std::array<u8, 2> bytes{};
    const GBAAudioFIFO* fifos[] = {&gba->audio.chA, &gba->audio.chB};
    for (unsigned fifo = 0; fifo < 2; ++fifo) {
        const auto& channel = *fifos[fifo];
        bytes[fifo] = static_cast<u8>(channel.internalRemaining ? channel.internalSample :
            channel.fifoRead != channel.fifoWrite ? channel.fifo[channel.fifoRead] : 0);
    }
    original_timers[Index](timing, context, late);
    *reference_events << cycle << ",1," << Index << ',' << gba->memory.io[(0x100 + Index * 4) / 2] << '\n';
    if constexpr (Index < 2) {
      if (gba->audio.enable) {
        if ((gba->audio.chALeft || gba->audio.chARight) && gba->audio.chATimer == Index)
            *reference_events << cycle << ",2,0," << static_cast<unsigned>(bytes[0]) << '\n';
        if ((gba->audio.chBLeft || gba->audio.chBRight) && gba->audio.chBTimer == Index)
            *reference_events << cycle << ",2,1," << static_cast<unsigned>(bytes[1]) << '\n';
      }
    }
}
void ReferenceDma(mTiming* timing, void* context, uint32_t late) {
    auto* gba = static_cast<GBA*>(context);
    const int active = gba->memory.activeDMA;
    if (active == 1 || active == 2) {
        const auto& dma = gba->memory.dma[active];
        if ((dma.dest == 0x040000A0 || dma.dest == 0x040000A4) && dma.nextCount == 4)
            *reference_events << static_cast<u32>(mTimingCurrentTime(timing) - late)
                << ",3," << active << ',' << dma.nextSource << '\n';
    }
    original_dma(timing, context, late);
}
#endif
constexpr u64 kRate = 48000, kClock = 16777216;
u64 Samples(u64 frames) { return frames * Ppu::kCyclesPerFrame * kRate / kClock; }
void Little(std::ostream& out, u32 value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) out.put(static_cast<char>(value >> (i * 8)));
}
void Wav(const std::filesystem::path& path, const std::vector<s16>& pcm) {
    std::ofstream out(path, std::ios::binary);
    const u32 size = static_cast<u32>(pcm.size() * 2);
    out << "RIFF"; Little(out, size + 36, 4); out << "WAVEfmt "; Little(out, 16, 4);
    Little(out, 1, 2); Little(out, 2, 2); Little(out, kRate, 4);
    Little(out, kRate * 4, 4); Little(out, 4, 2); Little(out, 16, 2);
    out << "data"; Little(out, size, 4);
    for (s16 sample : pcm) Little(out, static_cast<u16>(sample), 2);
    if (!out) throw std::runtime_error("Cannot write WAV");
}
template<class Write16, class Write32>
void Fixture(const std::string& name, Write16 write16, Write32 write32) {
    write16(0x04000084, 0);
    write16(0x04000084, 0x80);
    write16(0x04000088, 0x0200);
    write16(0x04000082, 2);
    if (name == "@square" || name == "@mixed") {
        write16(0x04000080, 0x1177);
        write16(0x04000062, 0xF080);
        write16(0x04000064, 0x8600); // 256 Hz, 50% duty.
    } else if (name == "@wave") {
        write16(0x04000070, 0x0040); // Select bank 1, fill bank 0.
        for (u32 offset = 0; offset < 16; offset += 2)
            write16(0x04000090 + offset, offset < 8 ? 0xFFFF : 0);
        write16(0x04000070, 0x0080);
        write16(0x04000072, 0x2000);
        write16(0x04000080, 0x4477);
        write16(0x04000074, 0x8700); // 256 Hz wave period.
    } else if (name == "@noise") {
        write16(0x04000080, 0x8877);
        write16(0x04000078, 0x8000);
        write16(0x0400007C, 0x8025);
    } else if (name != "@fifo") throw std::runtime_error("Unknown fixture");
    if (name == "@fifo" || name == "@mixed") {
        for (u32 offset = 0; offset < 0x20000; offset += 4) {
            const u32 a = static_cast<u8>(offset), b = static_cast<u8>(offset + 1);
            const u32 c = static_cast<u8>(offset + 2), d = static_cast<u8>(offset + 3);
            write32(0x02000000 + offset, a | (b << 8) | (c << 16) | (d << 24));
        }
        write16(0x04000082, 0x0B06); // Full FIFO A, both sides, reset.
        write32(0x040000BC, 0x02000000);
        write32(0x040000C0, 0x040000A0);
        write16(0x040000C4, 4);
        write16(0x040000C6, 0xB640);
        write16(0x04000100, 0xFC00); // 16384 FIFO bytes/sec: sawtooth at 64 Hz.
        write16(0x04000102, 0x80);
    }
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 4) throw std::runtime_error("usage: audio_diagnostics ROM FRAMES OUTPUT_DIR [--reference] [--start FRAME]");
        const unsigned frames = static_cast<unsigned>(std::stoul(argv[2]));
        if (frames < 8 || frames > 36000) throw std::runtime_error("FRAMES must be 8..36000");
        bool reference = false;
        bool events = false;
        std::string bios_path;
        unsigned start = 180;
        for (int i = 4; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--reference") reference = true;
            else if (arg == "--events") events = true;
            else if (arg == "--bios" && i + 1 < argc) bios_path = argv[++i];
            else if (arg == "--start" && i + 1 < argc) start = static_cast<unsigned>(std::stoul(argv[++i]));
            else throw std::runtime_error("Unknown option: " + arg);
        }
        const std::filesystem::path dir(argv[3]);
        std::filesystem::create_directories(dir / "captures");
        std::filesystem::create_directories(dir / "logs");
        const std::string source(argv[1]);
        const bool fixture = source.starts_with('@');
        const std::string name = reference ? "mgba" : "ours";
        std::ofstream log(dir / "logs" / (name + ".csv"));
        log << "frame,frame_ms,pc,soundcnt,bias,timer0,timer1,queued,hardware_ms,render_ms\n";
        std::vector<s16> pcm;
        pcm.reserve(Samples(frames) * 2);
        const auto begin = std::chrono::steady_clock::now();
        std::vector<u8> rom;
        if (fixture) {
            rom.resize(0x100);
            rom[0] = 0xFE; rom[1] = 0xFF; rom[2] = 0xFF; rom[3] = 0xEA;
        } else {
            std::ifstream input(argv[1], std::ios::binary);
            if (!input) throw std::runtime_error("Cannot read ROM");
            rom.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        }
        if (!reference) {
            std::vector<u8> bios;
            if (!bios_path.empty()) {
                std::ifstream input(bios_path, std::ios::binary);
                if (!input) throw std::runtime_error("Cannot read BIOS");
                bios.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
            }
            Emulator emu(std::move(rom), true, std::move(bios));
            emu.EnableTimingDiagnostics();
            std::ofstream event_log;
            if (events) {
                event_log.open(dir / "logs" / "ours-events.csv");
                event_log << "cycle,kind,index,value\n";
                emu.Memory().SetAudioTrace([](void* context, const Bus::AudioEvent& event) {
                    auto& stream = *static_cast<std::ofstream*>(context);
                    stream << event.cycle << ',' << static_cast<unsigned>(event.kind) << ','
                        << event.index << ',' << event.value << '\n';
                }, &event_log);
            }
            if (fixture) Fixture(source, [&](u32 a, u16 v) { emu.Memory().Write16(a, v); },
                [&](u32 a, u32 v) { emu.Memory().Write32(a, v); });
            AudioOutput output(emu.Memory());
            u64 rendered = 0;
            for (unsigned frame = 0; frame < frames; ++frame) {
                emu.Memory().SetKeyInput(frame >= start && frame < start + 4 ? 1u << 3 : 0);
                const auto before = std::chrono::steady_clock::now();
                emu.RunFrame();
                const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - before).count();
                auto& bus = emu.Memory();
                log << frame << ',' << ms << ',' << emu.Pc() << ',' << bus.Read16(0x04000082)
                    << ',' << bus.Read16(0x04000088) << ',' << bus.Read16(0x04000100)
                    << ',' << bus.Read16(0x04000104) << ',' << bus.QueuedAudioSamples()
                    << ',' << emu.LastFrameTiming().hardware_ms << ',' << emu.LastFrameTiming().render_ms << '\n';
                if (frame >= 4) {
                    const unsigned count = static_cast<unsigned>(Samples(frame - 3) - rendered);
                    const auto offset = pcm.size(); pcm.resize(offset + count * 2);
                    output.Fill(pcm.data() + offset, count); rendered += count;
                }
            }
            const unsigned tail = static_cast<unsigned>(Samples(frames) - rendered);
            const auto offset = pcm.size(); pcm.resize(offset + tail * 2);
            output.Fill(pcm.data() + offset, tail);
            std::cout << "underrun_samples=" << output.Underruns() << " dropped_samples=" << emu.Memory().DroppedAudioSamples() << '\n';
        } else {
#ifdef HAVE_MGBA
            if (std::string(projectVersion) != "0.10.5") throw std::runtime_error("mGBA 0.10.5 required");
            mCore* core = mCoreCreate(mPLATFORM_GBA);
            if (!core || !core->init(core)) throw std::runtime_error("mGBA init failed");
            mCoreInitConfig(core, nullptr);
            VFile* file = fixture ? VFileMemChunk(rom.data(), rom.size()) : VFileOpen(argv[1], O_RDONLY);
            if (!file || !core->loadROM(core, file)) throw std::runtime_error("mGBA ROM load failed");
            if (!bios_path.empty()) {
                VFile* bios = VFileOpen(bios_path.c_str(), O_RDONLY);
                if (!bios || !core->loadBIOS(core, bios, 0)) throw std::runtime_error("mGBA BIOS load failed");
            }
            std::array<color_t, 240 * 160> pixels{};
            core->setVideoBuffer(core, pixels.data(), 240);
            core->opts.skipBios = bios_path.empty();
            core->setAudioBufferSize(core, 4096);
            core->reset(core);
            std::ofstream event_log;
            if (events) {
                event_log.open(dir / "logs" / "mgba-events.csv");
                event_log << "cycle,kind,index,value\n";
                reference_events = &event_log;
                auto* gba = static_cast<GBA*>(core->board);
                const TimingCallback wrappers[] = {ReferenceTimer<0>, ReferenceTimer<1>, ReferenceTimer<2>, ReferenceTimer<3>};
                for (unsigned i = 0; i < 4; ++i) {
                    original_timers[i] = gba->timers[i].event.callback;
                    gba->timers[i].event.callback = wrappers[i];
                }
                original_dma = gba->memory.dmaEvent.callback;
                gba->memory.dmaEvent.callback = ReferenceDma;
            }
            auto* left = core->getAudioChannel(core, 0);
            auto* right = core->getAudioChannel(core, 1);
            blip_set_rates(left, core->frequency(core), kRate);
            blip_set_rates(right, core->frequency(core), kRate);
            if (fixture) Fixture(source, [&](u32 a, u16 v) { core->busWrite16(core, a, v); },
                [&](u32 a, u32 v) { core->busWrite32(core, a, v); });
            for (unsigned frame = 0; frame < frames; ++frame) {
                core->setKeys(core, frame >= start && frame < start + 4 ? 1u << 3 : 0);
                const auto before = std::chrono::steady_clock::now();
                core->runFrame(core);
                const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - before).count();
                u32 pc = 0; core->readRegister(core, "pc", &pc);
                log << frame << ',' << ms << ',' << pc << ',' << core->busRead16(core, 0x04000082)
                    << ',' << core->busRead16(core, 0x04000088) << ',' << core->busRead16(core, 0x04000100)
                    << ',' << core->busRead16(core, 0x04000104) << ',' << blip_samples_avail(left) << ",0,0\n";
                const unsigned count = static_cast<unsigned>(std::min(blip_samples_avail(left), blip_samples_avail(right)));
                const auto offset = pcm.size(); pcm.resize(offset + count * 2);
                blip_read_samples(left, pcm.data() + offset, count, 1);
                blip_read_samples(right, pcm.data() + offset + 1, count, 1);
            }
            core->deinit(core);
#else
            throw std::runtime_error("Configure with MGBA_ROOT to capture reference audio");
#endif
        }
        Wav(dir / "captures" / (name + ".wav"), pcm);
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        std::cout << name << " rate=" << kRate << " stereo_frames=" << pcm.size() / 2
            << " wall_seconds=" << elapsed << " emulated_seconds=" << static_cast<double>(frames) * Ppu::kCyclesPerFrame / kClock << '\n';
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
