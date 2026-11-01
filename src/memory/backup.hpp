#pragma once

#include <cstddef>
#include <vector>

#include "../types.hpp"

enum class BackupType { Auto, Sram, Flash64, Flash128, Eeprom512, Eeprom8K };

class Backup {
public:
    explicit Backup(const std::vector<u8>& rom);
    BackupType Type() const { return type_; }
    void Configure(BackupType type);
    void Load(const std::vector<u8>& bytes);
    const std::vector<u8>& Data() const { return data_; }
    bool Dirty() const { return dirty_; }
    void ClearDirty() { dirty_ = false; }

    u8 ReadByte(u32 address, u64 cycles) const;
    void WriteByte(u32 address, u8 value, u64 cycles);
    bool EepromMapped(u32 address) const;
    bool IsEeprom() const;
    void DetectEeprom();
    u16 ReadEepromBit(u64 cycles) const;
    void WriteEepromBit(u16 value, u32 remaining, u64 cycles);

private:
    enum class FlashState { Ready, Unlock2, Command, Program, Bank, EraseUnlock1,
                            EraseUnlock2, EraseCommand };
    BackupType type_ = BackupType::Auto;
    std::vector<u8> data_;
    std::size_t rom_size_ = 0;
    bool dirty_ = false;
    FlashState flash_state_ = FlashState::Ready;
    bool flash_id_ = false;
    unsigned flash_bank_ = 0;
    u64 flash_busy_until_ = 0;
    unsigned flash_settling_sector_ = 0;
    std::vector<u8> eeprom_input_;
    mutable std::vector<u8> eeprom_output_;
    mutable std::size_t eeprom_read_position_ = 0;
    u64 eeprom_busy_until_ = 0;
    void EraseFlash(std::size_t start, std::size_t size);
};
