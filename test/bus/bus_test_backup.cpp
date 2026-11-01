#include <catch2/catch_test_macros.hpp>

#include <string>

#include "memory/bus.hpp"

TEST_CASE("Flash identifies its chip, programs bytes, and switches 128 KiB banks") {
    Bus bus({});
    bus.ConfigureBackup(BackupType::Flash128);
    const auto command = [&](u8 value) {
        bus.Write8(0x0E005555, 0xAA);
        bus.Write8(0x0E002AAA, 0x55);
        bus.Write8(0x0E005555, value);
    };

    REQUIRE(bus.Read8(0x0E000123) == 0xFF);
    command(0x90);
    REQUIRE(bus.Read8(0x0E000000) == 0x62);
    REQUIRE(bus.Read8(0x0E000001) == 0x13);
    command(0xF0);
    command(0xA0);
    bus.Write8(0x0E000123, 0xA5);
    REQUIRE(bus.Read8(0x0E000123) == 0x5F);
    bus.AdvanceTimers(650);
    REQUIRE(bus.Read8(0x0E000123) == 0xA5);
    command(0xB0);
    bus.Write8(0x0E000000, 1);
    REQUIRE(bus.Read8(0x0E000123) == 0xFF);
    command(0xA0);
    bus.Write8(0x0E000123, 0x5A);
    bus.AdvanceTimers(650);
    command(0xB0);
    bus.Write8(0x0E000000, 0);
    REQUIRE(bus.Read8(0x0E000123) == 0xA5);
}

TEST_CASE("EEPROM DMA3 writes and reads a 64-bit block through one bit per halfword") {
    Bus bus({});
    bus.ConfigureBackup(BackupType::Eeprom512);
    const auto transfer = [&](u32 source, u32 destination, const std::vector<u8>& bits) {
        for (std::size_t i = 0; i < bits.size(); ++i)
            bus.Write16(source + static_cast<u32>(i * 2), bits[i]);
        bus.Write32(0x040000D4, source);
        bus.Write32(0x040000D8, destination);
        bus.Write32(0x040000DC, 0x80000000u | static_cast<u32>(bits.size()));
        bus.RunDma(0);
    };
    const u8 bytes[8] = {0xA5, 0x5A, 0x00, 0xFF, 0x81, 0x7E, 0x12, 0x34};
    std::vector<u8> write{1, 0, 0, 0, 0, 0, 0, 0};
    for (u8 byte : bytes)
        for (int bit = 7; bit >= 0; --bit) write.push_back((byte >> bit) & 1u);
    write.push_back(0);
    transfer(0x02000000, 0x0D000000, write);
    REQUIRE(bus.BackupDirty());
    REQUIRE(bus.Read16(0x0D000000) == 0); // Write is busy.

    transfer(0x02000100, 0x0D000000, {1, 1, 0, 0, 0, 0, 0, 0, 0});
    bus.Write32(0x040000D4, 0x0D000000);
    bus.Write32(0x040000D8, 0x02000200);
    bus.Write32(0x040000DC, 0x80000044); // Four dummy and 64 data bits.
    bus.RunDma(0);
    for (unsigned i = 0; i < 4; ++i) REQUIRE(bus.Read16(0x02000200 + i * 2) == 0);
    for (unsigned i = 0; i < 64; ++i)
        REQUIRE(bus.Read16(0x02000208 + i * 2) == ((bytes[i / 8] >> (7 - i % 8)) & 1u));
}

TEST_CASE("Flash sector erase preserves other sectors and chip erase clears both banks") {
    Bus bus({});
    bus.ConfigureBackup(BackupType::Flash128);
    const auto command = [&](u8 value) {
        bus.Write8(0x0E005555, 0xAA);
        bus.Write8(0x0E002AAA, 0x55);
        bus.Write8(0x0E005555, value);
    };
    const auto program = [&](u32 address, u8 value) {
        command(0xA0);
        bus.Write8(address, value);
        bus.AdvanceTimers(650);
    };
    program(0x0E001123, 0x12);
    program(0x0E002123, 0x34);
    command(0x80);
    bus.Write8(0x0E005555, 0xAA);
    bus.Write8(0x0E002AAA, 0x55);
    bus.Write8(0x0E001000, 0x30);
    REQUIRE(bus.Read8(0x0E001123) == 0x5F);
    bus.AdvanceTimers(30000);
    REQUIRE(bus.Read8(0x0E001123) == 0xFF);
    REQUIRE(bus.Read8(0x0E002123) == 0x34);
    command(0xB0);
    bus.Write8(0x0E000000, 1);
    program(0x0E000123, 0x56);
    command(0x80);
    bus.Write8(0x0E005555, 0xAA);
    bus.Write8(0x0E002AAA, 0x55);
    bus.Write8(0x0E005555, 0x10);
    REQUIRE(bus.Read8(0x0E000123) == 0xFF);
    command(0xB0);
    bus.Write8(0x0E000000, 0);
    REQUIRE(bus.Read8(0x0E002123) == 0xFF);
}

TEST_CASE("ROM save IDs select the device and unknown carts detect first backup write") {
    const auto rom = [](const char* id) {
        std::vector<u8> bytes(id, id + std::char_traits<char>::length(id));
        return bytes;
    };
    REQUIRE(Bus(rom("FLASH1M_V103")).GetBackupType() == BackupType::Flash128);
    REQUIRE(Bus(rom("FLASH512_V131")).GetBackupType() == BackupType::Flash64);
    REQUIRE(Bus(rom("EEPROM_V124")).GetBackupType() == BackupType::Eeprom512);
    REQUIRE(Bus(rom("SRAM_V113")).GetBackupType() == BackupType::Sram);
    Bus sram({});
    sram.Write8(0x0E000123, 0xA5);
    REQUIRE(sram.GetBackupType() == BackupType::Sram);
    REQUIRE(sram.Read8(0x0E000123) == 0xA5);
    Bus flash({});
    flash.Write8(0x0E005555, 0xAA);
    REQUIRE(flash.GetBackupType() == BackupType::Flash64);
}

TEST_CASE("EEPROM selects 8 KiB mode from a 14-bit DMA3 command") {
    Bus bus({});
    bus.ConfigureBackup(BackupType::Eeprom512);
    constexpr u16 block = 0x2A5;
    std::vector<u8> bits{1, 0};
    for (int bit = 13; bit >= 0; --bit) bits.push_back((block >> bit) & 1u);
    for (unsigned i = 0; i < 64; ++i) bits.push_back(i < 8 ? ((0xC3u >> (7 - i)) & 1u) : 1);
    bits.push_back(0);
    REQUIRE(bits.size() == 81);
    for (unsigned i = 0; i < bits.size(); ++i) bus.Write16(0x02000000 + i * 2, bits[i]);
    bus.Write32(0x040000D4, 0x02000000);
    bus.Write32(0x040000D8, 0x0D000000);
    bus.Write32(0x040000DC, 0x80000051);
    bus.RunDma(0);
    REQUIRE(bus.GetBackupType() == BackupType::Eeprom8K);
    REQUIRE(bus.BackupData().size() == 0x2000);
    REQUIRE(bus.BackupData()[block * 8] == 0xC3);
    REQUIRE(bus.BackupData()[block * 8 + 1] == 0xFF);
}
