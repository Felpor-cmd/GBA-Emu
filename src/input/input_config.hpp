#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <utility>
#include "../types.hpp"

enum class GbaButton : unsigned {
    A, B, Select, Start, Right, Left, Up, Down, R, L, Count
};

constexpr unsigned kGbaButtonCount = static_cast<unsigned>(GbaButton::Count);
constexpr u16 GbaButtonBit(GbaButton button) { return static_cast<u16>(1u << static_cast<unsigned>(button)); }

enum class BindingKind : unsigned char {
    Unbound, KeyboardKey, ControllerButton, ControllerAxis, ControllerHat
};

struct InputBinding {
    BindingKind kind = BindingKind::Unbound;
    int code = 0;
    int direction = 0;
    bool raw = false;
};

class InputConfig {
public:
    InputConfig();

    const InputBinding& KeyboardBinding(GbaButton button) const;
    const InputBinding& ControllerBinding(const std::string& guid, GbaButton button) const;
    void SetKeyboardBinding(GbaButton button, InputBinding binding);
    void SetControllerBinding(const std::string& guid, GbaButton button, InputBinding binding);
    void EnsureControllerProfile(const std::string& guid, bool mapped = true);
    const std::string& SelectedControllerGuid() const { return selected_controller_guid_; }
    void SetSelectedControllerGuid(std::string guid) { selected_controller_guid_ = std::move(guid); }

    bool Load(const std::string& path);
    bool Save(const std::string& path) const;

private:
    using Bindings = std::array<InputBinding, kGbaButtonCount>;
    std::array<InputBinding, kGbaButtonCount> keyboard_{};
    std::unordered_map<std::string, Bindings> controllers_;
    std::string selected_controller_guid_;
};
