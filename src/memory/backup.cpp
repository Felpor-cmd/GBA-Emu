#include "backup.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace {
bool Contains(const std::vector<u8>& data, std::string_view text) {
    return std::search(data.begin(), data.end(), text.begin(), text.end()) != data.end();
}

std::size_t SizeFor(BackupType type) {
    switch (type) {
        case BackupType::Sram: return 0x8000;
        case BackupType::Flash64: return 0x10000;
        case BackupType::Flash128: return 0x20000;
        case BackupType::Eeprom512: return 0x200;
        case BackupType::Eeprom8K: return 0x2000;
        case BackupType::Auto: return 0;
    }
    return 0;
}
}

Backup::Backup(const std::vector<u8>& rom) : rom_size_(rom.size()) {
    if (Contains(rom, "FLASH1M_V")) Configure(BackupType::Flash128);
    else if (Contains(rom, "FLASH512_V") || Contains(rom, "FLASH_V")) Configure(BackupType::Flash64);
    else if (Contains(rom, "EEPROM_V")) Configure(BackupType::Eeprom512);
    else if (Contains(rom, "SRAM_V")) Configure(BackupType::Sram);
}

void Backup::Configure(BackupType type) {
    type_ = type;
    data_.assign(SizeFor(type), 0xFF);
    dirty_ = false;
    flash_state_ = FlashState::Ready;
    flash_id_ = false;
    flash_bank_ = 0;
    flash_busy_until_ = 0;
    flash_settling_sector_ = 0;
    eeprom_input_.clear();
    eeprom_output_.clear();
    eeprom_read_position_ = 0;
    eeprom_busy_until_ = 0;
}

void Backup::Load(const std::vector<u8>& bytes) {
    if (type_ == BackupType::Auto) {
        switch (bytes.size()) {
            case 0x8000: Configure(BackupType::Sram); break;
            case 0x10000: Configure(BackupType::Flash64); break;
            case 0x20000: Configure(BackupType::Flash128); break;
            case 0x200: Configure(BackupType::Eeprom512); break;
            case 0x2000: Configure(BackupType::Eeprom8K); break;
            default: throw std::runtime_error("Unknown save file size");
        }
    } else if (type_ == BackupType::Eeprom512 && bytes.size() == 0x2000) {
        Configure(BackupType::Eeprom8K);
    }
    if (bytes.size() != data_.size())
        throw std::runtime_error("Save file size does not match cartridge backup type");
    data_ = bytes;
    dirty_ = false;
}

bool Backup::IsEeprom() const {
    return type_ == BackupType::Eeprom512 || type_ == BackupType::Eeprom8K;
}

bool Backup::EepromMapped(u32 address) const {
    return (address >> 24) == 0x0D &&
           (rom_size_ <= 0x1000000 || (address & 0x00FFFFFFu) >= 0x00FFFF00u);
}

void Backup::DetectEeprom() {
    if (type_ == BackupType::Auto) Configure(BackupType::Eeprom512);
}

u8 Backup::ReadByte(u32 address, u64 cycles) const {
    if (type_ == BackupType::Auto) return 0xFF;
    if (IsEeprom()) return 0xFF;
    const u32 offset = address & 0xFFFFu;
    if (flash_id_ && (type_ == BackupType::Flash64 || type_ == BackupType::Flash128) && offset < 2) {
        const u16 id = type_ == BackupType::Flash128 ? 0x1362 : 0x1B32;
        return static_cast<u8>(id >> (offset * 8));
    }
    if (type_ == BackupType::Sram) return data_[offset & 0x7FFFu];
    if (cycles < flash_busy_until_ && (offset >> 12) == flash_settling_sector_) return 0x5F;
    return data_[flash_bank_ * 0x10000u + offset];
}

void Backup::EraseFlash(std::size_t start, std::size_t size) {
    if (!std::all_of(data_.begin() + start, data_.begin() + start + size,
                     [](u8 byte) { return byte == 0xFF; })) dirty_ = true;
    std::fill(data_.begin() + start, data_.begin() + start + size, 0xFF);
}

void Backup::WriteByte(u32 address, u8 value, u64 cycles) {
    const u32 offset = address & 0xFFFFu;
    if (type_ == BackupType::Auto)
        Configure(offset == 0x5555u && value == 0xAA ? BackupType::Flash64 : BackupType::Sram);
    if (type_ == BackupType::Sram) {
        u8& byte = data_[offset & 0x7FFFu];
        dirty_ |= byte != value;
        byte = value;
        return;
    }
    if (type_ != BackupType::Flash64 && type_ != BackupType::Flash128) return;
    if (value == 0xF0 && offset == 0x5555 && flash_state_ != FlashState::Program) {
        flash_id_ = false;
        flash_state_ = FlashState::Ready;
        return;
    }
    switch (flash_state_) {
        case FlashState::Ready:
            if (offset == 0x5555 && value == 0xAA) flash_state_ = FlashState::Unlock2;
            break;
        case FlashState::Unlock2:
            flash_state_ = offset == 0x2AAA && value == 0x55 ? FlashState::Command : FlashState::Ready;
            break;
        case FlashState::Command:
            flash_state_ = FlashState::Ready;
            if (offset != 0x5555) break;
            if (value == 0x90) flash_id_ = true;
            else if (value == 0xA0) flash_state_ = FlashState::Program;
            else if (value == 0xB0 && type_ == BackupType::Flash128) flash_state_ = FlashState::Bank;
            else if (value == 0x80) flash_state_ = FlashState::EraseUnlock1;
            break;
        case FlashState::Program: {
            u8& byte = data_[flash_bank_ * 0x10000u + offset];
            const u8 programmed = byte & value;
            dirty_ |= byte != programmed;
            byte = programmed;
            flash_settling_sector_ = offset >> 12;
            flash_busy_until_ = cycles + 650;
            flash_state_ = FlashState::Ready;
            break;
        }
        case FlashState::Bank:
            if (offset == 0 && value < 2) flash_bank_ = value;
            flash_state_ = FlashState::Ready;
            break;
        case FlashState::EraseUnlock1:
            flash_state_ = offset == 0x5555 && value == 0xAA ? FlashState::EraseUnlock2 : FlashState::Ready;
            break;
        case FlashState::EraseUnlock2:
            flash_state_ = offset == 0x2AAA && value == 0x55 ? FlashState::EraseCommand : FlashState::Ready;
            break;
        case FlashState::EraseCommand:
            if (offset == 0x5555 && value == 0x10) EraseFlash(0, data_.size());
            else if ((offset & 0xFFFu) == 0 && value == 0x30) {
                EraseFlash(flash_bank_ * 0x10000u + (offset & 0xF000u), 0x1000);
                flash_settling_sector_ = offset >> 12;
                flash_busy_until_ = cycles + 30000;
            }
            flash_state_ = FlashState::Ready;
            break;
    }
}

void Backup::WriteEepromBit(u16 value, u32 remaining, u64 cycles) {
    DetectEeprom();
    if (!IsEeprom()) return;
    eeprom_input_.push_back(value & 1u);
    if (remaining != 1) return;
    const std::size_t count = eeprom_input_.size();
    if (count == 17 || count == 81) {
        if (type_ == BackupType::Eeprom512) {
            data_.resize(0x2000, 0xFF);
            type_ = BackupType::Eeprom8K;
        }
    }
    const std::size_t address_bits = type_ == BackupType::Eeprom8K ? 14 : 6;
    const bool read = count == address_bits + 3 && eeprom_input_[0] == 1 && eeprom_input_[1] == 1;
    const bool write = count == address_bits + 67 && eeprom_input_[0] == 1 && eeprom_input_[1] == 0;
    if (read || write) {
        std::size_t block = 0;
        for (std::size_t i = 0; i < address_bits; ++i) block = (block << 1) | eeprom_input_[i + 2];
        block &= type_ == BackupType::Eeprom8K ? 0x3FFu : 0x3Fu;
        if (read) {
            eeprom_output_.assign(4, 0);
            for (std::size_t i = 0; i < 64; ++i)
                eeprom_output_.push_back((data_[block * 8 + i / 8] >> (7 - i % 8)) & 1u);
            eeprom_read_position_ = 0;
        } else {
            for (std::size_t i = 0; i < 64; ++i) {
                u8& byte = data_[block * 8 + i / 8];
                const u8 mask = 1u << (7 - i % 8);
                const u8 updated = eeprom_input_[address_bits + 2 + i] ? byte | mask : byte & ~mask;
                dirty_ |= byte != updated;
                byte = updated;
            }
            eeprom_busy_until_ = cycles + 115000;
        }
    }
    eeprom_input_.clear();
}

u16 Backup::ReadEepromBit(u64 cycles) const {
    if (eeprom_read_position_ < eeprom_output_.size()) {
        const u16 bit = eeprom_output_[eeprom_read_position_++];
        if (eeprom_read_position_ == eeprom_output_.size()) eeprom_output_.clear(), eeprom_read_position_ = 0;
        return bit;
    }
    return cycles < eeprom_busy_until_ ? 0 : 1;
}
