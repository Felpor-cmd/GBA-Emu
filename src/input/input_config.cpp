#include "input_config.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <SDL2/SDL_gamecontroller.h>
#include <SDL2/SDL_scancode.h>

namespace {
constexpr unsigned Index(GbaButton button) { return static_cast<unsigned>(button); }

InputBinding Key(SDL_Scancode key) {
    return {BindingKind::KeyboardKey, static_cast<int>(key), 0, false};
}

std::array<InputBinding, kGbaButtonCount> DefaultControllerBindings() {
    std::array<InputBinding, kGbaButtonCount> bindings{};
    const auto button = [](SDL_GameControllerButton value) {
        return InputBinding{BindingKind::ControllerButton, static_cast<int>(value), 0, false};
    };
    bindings[Index(GbaButton::A)] = button(SDL_CONTROLLER_BUTTON_A);
    bindings[Index(GbaButton::B)] = button(SDL_CONTROLLER_BUTTON_B);
    bindings[Index(GbaButton::Select)] = button(SDL_CONTROLLER_BUTTON_BACK);
    bindings[Index(GbaButton::Start)] = button(SDL_CONTROLLER_BUTTON_START);
    bindings[Index(GbaButton::Right)] = button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    bindings[Index(GbaButton::Left)] = button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    bindings[Index(GbaButton::Up)] = button(SDL_CONTROLLER_BUTTON_DPAD_UP);
    bindings[Index(GbaButton::Down)] = button(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    bindings[Index(GbaButton::R)] = button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    bindings[Index(GbaButton::L)] = button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    return bindings;
}

std::array<InputBinding, kGbaButtonCount> DefaultGenericBindings() {
    std::array<InputBinding, kGbaButtonCount> bindings{};
    const auto button = [](int index) { return InputBinding{BindingKind::ControllerButton, index, 0, true}; };
    const auto hat = [](int direction) { return InputBinding{BindingKind::ControllerHat, 0, direction, true}; };
    bindings[Index(GbaButton::A)] = button(0);
    bindings[Index(GbaButton::B)] = button(1);
    bindings[Index(GbaButton::Select)] = button(8);
    bindings[Index(GbaButton::Start)] = button(9);
    bindings[Index(GbaButton::Right)] = hat(SDL_HAT_RIGHT);
    bindings[Index(GbaButton::Left)] = hat(SDL_HAT_LEFT);
    bindings[Index(GbaButton::Up)] = hat(SDL_HAT_UP);
    bindings[Index(GbaButton::Down)] = hat(SDL_HAT_DOWN);
    bindings[Index(GbaButton::R)] = button(5);
    bindings[Index(GbaButton::L)] = button(4);
    return bindings;
}

bool ParseInt(const std::string& text, int& value) {
    try {
        std::size_t used = 0;
        value = std::stoi(text, &used);
        return used == text.size();
    } catch (...) {
        return false;
    }
}

bool ParseBinding(const std::vector<std::string>& fields, std::size_t offset,
                  InputBinding& binding) {
    int kind = 0, code = 0, direction = 0, raw = 0;
    if (fields.size() < offset + 4 || !ParseInt(fields[offset], kind) ||
        !ParseInt(fields[offset + 1], code) || !ParseInt(fields[offset + 2], direction) ||
        !ParseInt(fields[offset + 3], raw)) return false;
    if (kind < static_cast<int>(BindingKind::Unbound) ||
        kind > static_cast<int>(BindingKind::ControllerHat) || (raw != 0 && raw != 1)) return false;
    binding = {static_cast<BindingKind>(kind), code, direction, raw != 0};
    return true;
}

void WriteBinding(std::ostream& output, const char* group, const std::string& owner,
                  unsigned button, const InputBinding& binding, bool has_owner) {
    output << group;
    if (has_owner) output << '|' << owner;
    output << '|' << button << '|' << static_cast<unsigned>(binding.kind) << '|'
           << binding.code << '|' << binding.direction << '|' << (binding.raw ? 1 : 0) << '\n';
}
}  // namespace

InputConfig::InputConfig() {
    keyboard_[Index(GbaButton::A)] = Key(SDL_SCANCODE_J);
    keyboard_[Index(GbaButton::B)] = Key(SDL_SCANCODE_K);
    keyboard_[Index(GbaButton::Select)] = Key(SDL_SCANCODE_RSHIFT);
    keyboard_[Index(GbaButton::Start)] = Key(SDL_SCANCODE_RETURN);
    keyboard_[Index(GbaButton::Right)] = Key(SDL_SCANCODE_D);
    keyboard_[Index(GbaButton::Left)] = Key(SDL_SCANCODE_A);
    keyboard_[Index(GbaButton::Up)] = Key(SDL_SCANCODE_W);
    keyboard_[Index(GbaButton::Down)] = Key(SDL_SCANCODE_S);
    keyboard_[Index(GbaButton::R)] = Key(SDL_SCANCODE_I);
    keyboard_[Index(GbaButton::L)] = Key(SDL_SCANCODE_U);
}

void InputConfig::SetFastForwardSpeed(unsigned speed) {
    if (speed == 1 || speed == 2 || speed == 3 || speed == 5 || speed == 10)
        fast_forward_speed_ = speed;
}

const InputBinding& InputConfig::KeyboardBinding(GbaButton button) const {
    return keyboard_[Index(button)];
}

const InputBinding& InputConfig::ControllerBinding(const std::string& guid, GbaButton button) const {
    const auto profile = controllers_.find(guid);
    if (profile != controllers_.end()) return profile->second[Index(button)];
    static const InputBinding unbound{};
    return unbound;
}

void InputConfig::SetKeyboardBinding(GbaButton button, InputBinding binding) {
    keyboard_[Index(button)] = binding;
}

void InputConfig::EnsureControllerProfile(const std::string& guid, bool mapped) {
    if (!guid.empty()) controllers_.try_emplace(guid, mapped ? DefaultControllerBindings() : DefaultGenericBindings());
}

void InputConfig::SetControllerBinding(const std::string& guid, GbaButton button,
                                       InputBinding binding) {
    EnsureControllerProfile(guid);
    if (!guid.empty()) controllers_[guid][Index(button)] = binding;
}

bool InputConfig::Load(const std::string& path) {
    std::ifstream input(path);
    if (!input) return false;
    std::string line;
    if (!std::getline(input, line) || line != "gba-input-config-v1") return false;

    InputConfig loaded;
    while (std::getline(input, line)) {
        std::vector<std::string> fields;
        std::istringstream record(line);
        std::string field;
        while (std::getline(record, field, '|')) fields.push_back(field);
        if (fields.empty()) continue;
        if (fields[0] == "selected" && fields.size() == 2) {
            loaded.selected_controller_guid_ = fields[1];
            continue;
        }
        if (fields[0] == "fast_forward_speed" && fields.size() == 2) {
            int speed = 0;
            if (ParseInt(fields[1], speed) && speed > 0)
                loaded.SetFastForwardSpeed(static_cast<unsigned>(speed));
            continue;
        }
        if (fields[0] == "keyboard" && fields.size() == 6) {
            int button = 0;
            InputBinding binding;
            if (ParseInt(fields[1], button) && button >= 0 && button < static_cast<int>(kGbaButtonCount) &&
                ParseBinding(fields, 2, binding))
                loaded.keyboard_[static_cast<unsigned>(button)] = binding;
            continue;
        }
        if (fields[0] == "controller" && fields.size() == 7) {
            int button = 0;
            InputBinding binding;
            if (!fields[1].empty() && ParseInt(fields[2], button) && button >= 0 &&
                button < static_cast<int>(kGbaButtonCount) && ParseBinding(fields, 3, binding)) {
                loaded.EnsureControllerProfile(fields[1]);
                loaded.controllers_[fields[1]][static_cast<unsigned>(button)] = binding;
            }
        }
    }
    *this = std::move(loaded);
    return true;
}

bool InputConfig::Save(const std::string& path) const {
    try {
        const std::filesystem::path file(path);
        if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path());
        std::ofstream output(file, std::ios::trunc);
        if (!output) return false;
        output << "gba-input-config-v1\nselected|" << selected_controller_guid_ << '\n';
        output << "fast_forward_speed|" << fast_forward_speed_ << '\n';
        for (unsigned i = 0; i < kGbaButtonCount; ++i)
            WriteBinding(output, "keyboard", {}, i, keyboard_[i], false);

        std::vector<std::string> guids;
        guids.reserve(controllers_.size());
        for (const auto& [guid, unused] : controllers_) {
            (void)unused;
            guids.push_back(guid);
        }
        std::sort(guids.begin(), guids.end());
        for (const auto& guid : guids)
            for (unsigned i = 0; i < kGbaButtonCount; ++i)
                WriteBinding(output, "controller", guid, i, controllers_.at(guid)[i], true);
        return static_cast<bool>(output);
    } catch (...) {
        return false;
    }
}
