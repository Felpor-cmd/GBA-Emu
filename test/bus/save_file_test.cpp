#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

#include "memory/bus.hpp"
#include "memory/save_file.hpp"

TEST_CASE("Save file survives a new emulator bus and rejects incompatible data") {
    const auto path = std::filesystem::temp_directory_path() /
        ("gba-emu-save-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".sav");
    SaveFile file(path);
    {
        Bus first({});
        first.ConfigureBackup(BackupType::Sram);
        first.Write8(0x0E000123, 0x5A);
        REQUIRE(first.BackupDirty());
        file.Flush(first);
        REQUIRE_FALSE(first.BackupDirty());
        REQUIRE(std::filesystem::file_size(path) == 0x8000);
    }
    {
        Bus second({});
        file.Load(second);
        REQUIRE(second.GetBackupType() == BackupType::Sram);
        REQUIRE(second.Read8(0x0E000123) == 0x5A);
        REQUIRE_FALSE(second.BackupDirty());
    }
    {
        Bus incompatible({});
        incompatible.ConfigureBackup(BackupType::Flash128);
        REQUIRE_THROWS(file.Load(incompatible));
        REQUIRE(std::filesystem::file_size(path) == 0x8000);
    }
    std::filesystem::remove(path);
}

TEST_CASE("Flash and EEPROM backup images round trip through raw save files") {
    const auto base = std::filesystem::temp_directory_path() /
        ("gba-emu-chip-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto flash_path = std::filesystem::path(base.string() + "-flash.sav");
    const auto eeprom_path = std::filesystem::path(base.string() + "-eeprom.sav");
    {
        Bus flash({});
        flash.ConfigureBackup(BackupType::Flash128);
        flash.Write8(0x0E005555, 0xAA);
        flash.Write8(0x0E002AAA, 0x55);
        flash.Write8(0x0E005555, 0xA0);
        flash.Write8(0x0E000123, 0x5A);
        SaveFile(flash_path).Flush(flash);
        Bus reopened({});
        SaveFile(flash_path).Load(reopened);
        REQUIRE(reopened.GetBackupType() == BackupType::Flash128);
        REQUIRE(reopened.Read8(0x0E000123) == 0x5A);
    }
    {
        Bus eeprom(std::vector<u8>{'E', 'E', 'P', 'R', 'O', 'M', '_', 'V'});
        eeprom.ConfigureBackup(BackupType::Eeprom8K);
        std::vector<u8> bits{1, 0};
        for (unsigned i = 0; i < 14; ++i) bits.push_back(0);
        for (unsigned i = 0; i < 64; ++i) bits.push_back(i < 8 ? ((0xA5u >> (7 - i)) & 1u) : 1);
        bits.push_back(0);
        for (unsigned i = 0; i < bits.size(); ++i)
            eeprom.Write16(0x02000000 + i * 2, bits[i]);
        eeprom.Write32(0x040000D4, 0x02000000);
        eeprom.Write32(0x040000D8, 0x0D000000);
        eeprom.Write32(0x040000DC, 0x80000051);
        eeprom.RunDma(0);
        SaveFile(eeprom_path).Flush(eeprom);
        Bus reopened(std::vector<u8>{'E', 'E', 'P', 'R', 'O', 'M', '_', 'V'});
        SaveFile(eeprom_path).Load(reopened);
        REQUIRE(reopened.GetBackupType() == BackupType::Eeprom8K);
        REQUIRE(reopened.BackupData()[0] == 0xA5);
        REQUIRE(reopened.BackupData().size() == 0x2000);
    }
    std::filesystem::remove(flash_path);
    std::filesystem::remove(eeprom_path);
}

TEST_CASE("Failed save write leaves dirty data available for retry") {
    const auto path = std::filesystem::temp_directory_path() /
        ("gba-emu-missing-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())) /
        "game.sav";
    Bus bus({});
    bus.Write8(0x0E000123, 0x42);
    REQUIRE_THROWS(SaveFile(path).Flush(bus));
    REQUIRE(bus.BackupDirty());
    REQUIRE_FALSE(std::filesystem::exists(path));
}
