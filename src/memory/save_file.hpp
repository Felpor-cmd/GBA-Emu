#pragma once

#include <filesystem>

class Bus;

// A raw cartridge backup image stored beside the ROM unless a path is supplied.
class SaveFile {
public:
    explicit SaveFile(std::filesystem::path path) : path_(std::move(path)) {}
    void Load(Bus& bus) const;
    void Flush(Bus& bus) const;

private:
    std::filesystem::path path_;
};
