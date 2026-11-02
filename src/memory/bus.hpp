#pragma once
#include <array>
#include <vector>
#include "../types.hpp"
#include "../sound/sound.hpp"
#include "backup.hpp"

// Region sizes from the GBA memory map (GBATEK: GBA Memory Map).
constexpr u32 kBiosSize    = 0x4000;    // 16 KB  BIOS ROM
constexpr u32 kEwramSize   = 0x40000;   // 256 KB on-board WRAM
constexpr u32 kIwramSize   = 0x8000;    // 32 KB  in-chip WRAM
constexpr u32 kPaletteSize = 0x400;     // 1 KB   BG/OBJ palette RAM
constexpr u32 kVramSize    = 0x18000;   // 96 KB  video RAM
constexpr u32 kOamSize     = 0x400;     // 1 KB   object attribute memory
constexpr u32 kSramSize    = 0x8000;   // 32 KB  static RAM

constexpr u32 kIoSize      = 0x400;     // 1 KB   I/O Registers - PLACEHOLDER

// Owns RAM, I/O storage, BIOS data, and the loaded ROM. CPU and PPU access
// memory through this shared address map.
class Bus {
public:
    enum class AudioEventKind { RegisterWrite, TimerOverflow, FifoSample, DmaRefill };
    struct AudioEvent { u64 cycle; AudioEventKind kind; unsigned index; u32 value; };
    using AudioTrace = void (*)(void*, const AudioEvent&);
    void SetAudioTrace(AudioTrace trace, void* context) { audio_trace_ = trace; audio_trace_context_ = context; }
    explicit Bus(std::vector<u8> rom, std::vector<u8> bios = {});

    u8  Read8(u32 address) const;
    u16 Read16(u32 address) const;
    u32 Read32(u32 address) const;

    void Write8(u32 address, u8 value);
    void Write16(u32 address, u16 value);
    void Write32(u32 address, u32 value);

    // Number of 16.78 MHz bus clocks for one CPU access. `sequential` is
    // supplied by the CPU's instruction/data access schedule.
    u32 AccessCycles(u32 address, unsigned width, bool sequential) const;
    bool PrefetchEnabled() const { return (waitcnt_ & (1u << 14)) != 0; }
    u16 GetWaitcnt() const { return waitcnt_; }
    void SetVideoTiming(u16 scanline, bool hblank);
    // `pressed` uses one bits for pressed GBA buttons; KEYINPUT reads active-low.
    void SetKeyInput(u16 pressed);
    void ResetMemory(u32 flags);
    u32 RunDma(unsigned timing);
    u32 AdvanceTimers(u32 cycles);
    u32 CyclesToTimerEvent() const;
    u64 HardwareCycles() const { return hardware_cycles_; }
    bool PopAudioSample(s16& left, s16& right);
    u32 QueuedAudioSamples() const { return sound_.QueuedSamples(); }
    u64 DroppedAudioSamples() const { return sound_.DroppedSamples(); }
    void RequestInterrupt(u16 bits);
    bool InterruptPending() const { return (Read16(0x04000200) & Read16(0x04000202)) != 0; }
    bool IrqEnabled() const { return (Read16(0x04000208) & 1u) && InterruptPending(); }
    bool HasBios() const { return has_bios_; }
    // The BIOS data bus is readable only while executing BIOS code. Outside
    // it, reads expose the last prefetched BIOS instruction.
    void SetCpuExecutionAddress(u32 address) { cpu_in_bios_ = has_bios_ && address < kBiosSize; }
    void LatchBiosPrefetch(u32 address, bool thumb);
    void LatchOpenBusPrefetch(u32 address, bool thumb);
    bool HaltRequested() const { return halt_requested_; }
    void ClearHaltRequest() { halt_requested_ = false; }
    BackupType GetBackupType() const { return backup_.Type(); }
    void ConfigureBackup(BackupType type) { backup_.Configure(type); }
    void LoadBackup(const std::vector<u8>& bytes) { backup_.Load(bytes); }
    const std::vector<u8>& BackupData() const { return backup_.Data(); }
    bool BackupDirty() const { return backup_.Dirty(); }
    void ClearBackupDirty() { backup_.ClearDirty(); }

private:
    std::array<u8, kBiosSize>    bios_{};
    std::array<u8, kEwramSize>   ewram_{};
    std::array<u8, kIwramSize>   iwram_{};
    std::array<u8, kPaletteSize> palette_{};
    std::array<u8, kVramSize>    vram_{};
    std::array<u8, kOamSize>     oam_{};
    std::array<u8, kIoSize>      io_{};
    mutable Backup backup_;
    std::vector<u8>              rom_;  
    Sound sound_{};
    u16 waitcnt_ = 0;
    bool has_bios_ = false;
    bool cpu_in_bios_ = false;
    bool halt_requested_ = false;
    u32 bios_prefetch_ = 0;
    u32 OpenBusPrefetch() const;
    mutable u32 open_bus_prefetch_ = 0;
    mutable bool open_bus_prefetch_pending_ = false;
    u32 open_bus_execution_address_ = 0;
    bool open_bus_thumb_ = false;
    struct Dma { u32 source = 0, destination = 0, reload = 0; bool enabled = false; };
    std::array<Dma, 4> dma_{};
    struct Timer {
        u16 reload = 0;
        u16 counter = 0;
        u8 control = 0;
        u16 prescaler_phase = 0;
    };
    std::array<Timer, 4> timers_{};
    u64 hardware_cycles_ = 0;
    AudioTrace audio_trace_ = nullptr;
    void* audio_trace_context_ = nullptr;
    void TraceAudio(AudioEventKind kind, unsigned index, u32 value) const {
        if (audio_trace_) audio_trace_(audio_trace_context_, {hardware_cycles_, kind, index, value});
    }
    u64 IncrementTimer(unsigned timer, u64 ticks);
    u32 RunSoundDma(u8 requests);
};
