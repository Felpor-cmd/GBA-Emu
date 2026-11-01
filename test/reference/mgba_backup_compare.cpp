#include <mgba/core/timing.h>
#include <mgba/internal/gba/savedata.h>

#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "memory/bus.hpp"

namespace {
struct Reference {
    int32_t relative = 0;
    int32_t next = 0;
    mTiming timing{};
    GBASavedata save{};
    explicit Reference(SavedataType type) {
        mTimingInit(&timing, &relative, &next);
        save.timing = &timing;
        GBASavedataInit(&save, nullptr);
        GBASavedataForceType(&save, type);
    }
    ~Reference() {
        GBASavedataDeinit(&save);
        mTimingDeinit(&timing);
    }
};

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void CompareFlash(BackupType type, SavedataType reference_type) {
    Bus bus({});
    bus.ConfigureBackup(type);
    Reference reference(reference_type);
    const auto write = [&](u16 address, u8 value) {
        bus.Write8(0x0E000000u + address, value);
        GBASavedataWriteFlash(&reference.save, address, value);
    };
    const auto command = [&](u8 value) {
        write(0x5555, 0xAA);
        write(0x2AAA, 0x55);
        write(0x5555, value);
    };
    const auto compare = [&](u16 address) {
        Check(bus.Read8(0x0E000000u + address) ==
              GBASavedataReadFlash(&reference.save, address), "Flash read differs from mGBA");
    };
    const auto advance = [&](u32 cycles) {
        bus.AdvanceTimers(cycles);
        mTimingTick(&reference.timing, cycles);
    };
    command(0x90);
    compare(0);
    compare(1);
    command(0xF0);
    command(0xA0);
    write(0x1234, 0xA5);
    advance(650);
    compare(0x1234);
    command(0x80);
    write(0x5555, 0xAA);
    write(0x2AAA, 0x55);
    write(0x1000, 0x30);
    advance(30000);
    compare(0x1234);
    if (type == BackupType::Flash128) {
        command(0xB0);
        write(0, 1);
        compare(0x1234);
        command(0xA0);
        write(0x1234, 0x5A);
        advance(650);
        compare(0x1234);
        command(0xB0);
        write(0, 0);
        compare(0x1234);
    }
}

void TransferToEeprom(Bus& bus, const std::vector<u8>& bits) {
    for (std::size_t i = 0; i < bits.size(); ++i)
        bus.Write16(0x02000000u + static_cast<u32>(i * 2), bits[i]);
    bus.Write32(0x040000D4, 0x02000000);
    bus.Write32(0x040000D8, 0x0D000000);
    bus.Write32(0x040000DC, 0x80000000u | static_cast<u32>(bits.size()));
    bus.RunDma(0);
}

void CompareEeprom() {
    Bus bus({});
    bus.ConfigureBackup(BackupType::Eeprom512);
    Reference reference(SAVEDATA_EEPROM512);
    std::vector<u8> write{1, 0, 0, 0, 0, 0, 0, 0};
    constexpr std::array<u8, 8> bytes{0xA5, 0x5A, 0x00, 0xFF, 0x81, 0x7E, 0x12, 0x34};
    for (u8 byte : bytes)
        for (int bit = 7; bit >= 0; --bit) write.push_back((byte >> bit) & 1u);
    write.push_back(0);
    TransferToEeprom(bus, write);
    for (std::size_t i = 0; i < write.size(); ++i)
        GBASavedataWriteEEPROM(&reference.save, write[i], write.size() - i);
    for (std::size_t i = 0; i < 512; ++i)
        Check(bus.BackupData()[i] == reference.save.data[i], "EEPROM data differs from mGBA");

    const std::vector<u8> read{1, 1, 0, 0, 0, 0, 0, 0, 0};
    TransferToEeprom(bus, read);
    for (std::size_t i = 0; i < read.size(); ++i)
        GBASavedataWriteEEPROM(&reference.save, read[i], read.size() - i);
    for (unsigned i = 0; i < 68; ++i)
        Check(bus.Read16(0x0D000000) == GBASavedataReadEEPROM(&reference.save),
              "EEPROM serial read differs from mGBA");
}
}

int main() {
    try {
        CompareFlash(BackupType::Flash64, SAVEDATA_FLASH512);
        CompareFlash(BackupType::Flash128, SAVEDATA_FLASH1M);
        CompareEeprom();
        std::cout << "Flash and EEPROM transactions match mGBA 0.10.5\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
