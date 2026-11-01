#include "save_file.hpp"

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "bus.hpp"

void SaveFile::Load(Bus& bus) const {
    std::error_code error;
    if (!std::filesystem::exists(path_, error)) {
        if (error) throw std::runtime_error("Cannot inspect save file: " + path_.string());
        return;
    }
    std::ifstream input(path_, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() < 0) throw std::runtime_error("Cannot read save file: " + path_.string());
    std::vector<u8> bytes(static_cast<std::size_t>(input.tellg()));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
        throw std::runtime_error("Cannot read save file: " + path_.string());
    bus.LoadBackup(bytes);
}

void SaveFile::Flush(Bus& bus) const {
    if (!bus.BackupDirty() || bus.BackupData().empty()) return;
    const auto temporary = std::filesystem::path(path_.string() + ".tmp." +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Cannot write save file: " + path_.string());
        const auto& data = bus.BackupData();
        output.write(reinterpret_cast<const char*>(data.data()), data.size());
        output.flush();
        if (!output) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            throw std::runtime_error("Cannot write save file: " + path_.string());
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, path_, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        throw std::runtime_error("Cannot replace save file: " + path_.string());
    }
    bus.ClearBackupDirty();
}
