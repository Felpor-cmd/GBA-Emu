#include "input_manager.hpp"

#include <algorithm>
#include <cstdlib>

namespace {
constexpr Sint16 kAxisThreshold = 16000;

std::string GuidString(SDL_JoystickGUID guid) {
    char text[33]{};
    SDL_JoystickGetGUIDString(guid, text, sizeof(text));
    return text;
}

std::string ControllerType(SDL_GameControllerType type) {
    switch (type) {
        case SDL_CONTROLLER_TYPE_XBOX360: case SDL_CONTROLLER_TYPE_XBOXONE: return "Xbox controller";
        case SDL_CONTROLLER_TYPE_PS3: case SDL_CONTROLLER_TYPE_PS4: case SDL_CONTROLLER_TYPE_PS5:
            return "PlayStation controller";
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return "Nintendo Switch controller";
        default: return "Generic controller";
    }
}
}

InputManager::InputManager(std::string config_path) : config_path_(std::move(config_path)) {}

InputManager::~InputManager() { Shutdown(); }

bool InputManager::Initialize() {
    if (initialized_) return true;
    const Uint32 initialized = SDL_WasInit(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    const Uint32 needed = (SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) & ~initialized;
    if (needed && SDL_InitSubSystem(needed) != 0) return false;
    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);
    if (config_path_.empty()) {
        char* pref = SDL_GetPrefPath("gba-emu", "gba-emu");
        if (pref) {
            config_path_ = std::string(pref) + "input.ini";
            SDL_free(pref);
        }
    }
    if (!config_path_.empty()) config_.Load(config_path_);
    initialized_ = true;
    RefreshDevices();
    return true;
}

void InputManager::Shutdown() {
    CloseSelected();
    devices_.clear();
    selected_device_ = -1;
    initialized_ = false;
}

void InputManager::RefreshDevices() {
    std::string wanted = SelectedGuid();
    const SDL_JoystickID old_instance = selected_device_ >= 0 && selected_device_ < static_cast<int>(devices_.size())
        ? devices_[selected_device_].instance : -1;
    const int count = SDL_NumJoysticks();
    std::vector<InputDeviceInfo> found;
    found.reserve(std::max(count, 0));
    for (int i = 0; i < count; ++i) {
        SDL_Joystick* probe = SDL_JoystickOpen(i);
        if (!probe) continue;
        const SDL_JoystickID instance = SDL_JoystickInstanceID(probe);
        const std::string guid = GuidString(SDL_JoystickGetGUID(probe));
        const bool mapped = SDL_IsGameController(i) == SDL_TRUE;
        const char* name = mapped ? SDL_GameControllerNameForIndex(i) : SDL_JoystickName(probe);
        const SDL_GameControllerType type = mapped ? SDL_GameControllerTypeForIndex(i) : SDL_CONTROLLER_TYPE_UNKNOWN;
        found.push_back({instance, guid, name ? name : "Controller", mapped ? ControllerType(type) : "Generic controller", mapped});
        SDL_JoystickClose(probe);
    }
    devices_ = std::move(found);
    int selected = -1;
    if (!wanted.empty()) {
        for (std::size_t i = 0; i < devices_.size(); ++i)
            if (devices_[i].guid == wanted) { selected = static_cast<int>(i); break; }
    }
    if (selected < 0 && !devices_.empty()) selected = 0;
    const SDL_JoystickID new_instance = selected >= 0 ? devices_[selected].instance : -1;
    if (selected != selected_device_ || new_instance != old_instance ||
        (selected >= 0 && !controller_ && !joystick_)) {
        CloseSelected();
        selected_device_ = selected;
        if (selected_device_ >= 0) {
            config_.SetSelectedControllerGuid(devices_[selected_device_].guid);
            config_.EnsureControllerProfile(devices_[selected_device_].guid, devices_[selected_device_].mapped);
        } else config_.SetSelectedControllerGuid({});
        OpenSelected();
        Save();
    }
}

void InputManager::CloseSelected() {
    if (controller_) SDL_GameControllerClose(controller_);
    else if (joystick_) SDL_JoystickClose(joystick_);
    controller_ = nullptr;
    joystick_ = nullptr;
    joystick_instance_ = -1;
}

void InputManager::OpenSelected() {
    if (selected_device_ < 0 || selected_device_ >= static_cast<int>(devices_.size())) return;
    const auto instance = devices_[selected_device_].instance;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        SDL_Joystick* probe = SDL_JoystickOpen(i);
        if (!probe) continue;
        const bool match = SDL_JoystickInstanceID(probe) == instance;
        SDL_JoystickClose(probe);
        if (!match) continue;
        if (devices_[selected_device_].mapped) controller_ = SDL_GameControllerOpen(i);
        if (!controller_) joystick_ = SDL_JoystickOpen(i);
        break;
    }
    joystick_instance_ = instance;
}

std::string InputManager::SelectedGuid() const {
    if (selected_device_ >= 0 && selected_device_ < static_cast<int>(devices_.size()))
        return devices_[selected_device_].guid;
    return config_.SelectedControllerGuid();
}

void InputManager::SelectDevice(int index) {
    if (index < -1 || index >= static_cast<int>(devices_.size())) return;
    if (index == selected_device_) return;
    CloseSelected();
    selected_device_ = index;
    config_.SetSelectedControllerGuid(index >= 0 ? devices_[index].guid : std::string{});
    if (index >= 0) config_.EnsureControllerProfile(devices_[index].guid, devices_[index].mapped);
    OpenSelected();
    Save();
}

void InputManager::ProcessEvent(const SDL_Event& event) {
    if (event.type == SDL_CONTROLLERDEVICEADDED || event.type == SDL_JOYDEVICEADDED) {
        RefreshDevices();
        return;
    }
    if (event.type == SDL_CONTROLLERDEVICEREMOVED || event.type == SDL_JOYDEVICEREMOVED) {
        if (event.cdevice.which == joystick_instance_) CloseSelected();
        RefreshDevices();
        return;
    }
    if (!rebinding_) return;
    if (event.type == SDL_KEYDOWN && !event.key.repeat) {
        if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) { CancelRebind(); return; }
        if (!rebind_controller_) {
            config_.SetKeyboardBinding(rebind_button_, {BindingKind::KeyboardKey,
                static_cast<int>(event.key.keysym.scancode), 0, false});
            rebinding_ = false;
            Save();
        }
    } else if (rebind_controller_) {
        InputBinding binding;
        if (event.type == SDL_CONTROLLERBUTTONDOWN && controller_ && event.cbutton.which == joystick_instance_) {
            binding = {BindingKind::ControllerButton, event.cbutton.button, 0, false};
        } else if (event.type == SDL_JOYBUTTONDOWN && joystick_ && event.jbutton.which == joystick_instance_) {
            binding = {BindingKind::ControllerButton, event.jbutton.button, 0, true};
        } else if (event.type == SDL_CONTROLLERAXISMOTION && controller_ && event.caxis.which == joystick_instance_ && std::abs(event.caxis.value) >= kAxisThreshold) {
            binding = {BindingKind::ControllerAxis, event.caxis.axis, event.caxis.value > 0 ? 1 : -1, false};
        } else if (event.type == SDL_JOYAXISMOTION && joystick_ && event.jaxis.which == joystick_instance_ && std::abs(event.jaxis.value) >= kAxisThreshold) {
            binding = {BindingKind::ControllerAxis, event.jaxis.axis, event.jaxis.value > 0 ? 1 : -1, true};
        } else if (event.type == SDL_JOYHATMOTION && joystick_ && event.jhat.which == joystick_instance_ && event.jhat.value != SDL_HAT_CENTERED) {
            binding = {BindingKind::ControllerHat, event.jhat.hat, event.jhat.value, true};
        }
        if (binding.kind != BindingKind::Unbound) {
            config_.SetControllerBinding(SelectedGuid(), rebind_button_, binding);
            rebinding_ = false;
            Save();
        }
    }
}

bool InputManager::BindingPressed(const InputBinding& binding) const {
    if (binding.kind == BindingKind::Unbound) return false;
    if (binding.kind == BindingKind::KeyboardKey) {
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        return binding.code >= 0 && binding.code < SDL_NUM_SCANCODES && keys[binding.code] != 0;
    }
    if (!controller_ && !joystick_) return false;
    const bool raw = binding.raw || !controller_;
    switch (binding.kind) {
        case BindingKind::ControllerButton:
            return raw ? (joystick_ && binding.code >= 0 && binding.code < SDL_JoystickNumButtons(joystick_) && SDL_JoystickGetButton(joystick_, binding.code))
                       : (binding.code >= 0 && binding.code < SDL_CONTROLLER_BUTTON_MAX && SDL_GameControllerGetButton(controller_, static_cast<SDL_GameControllerButton>(binding.code)));
        case BindingKind::ControllerAxis: {
            Sint16 value = 0;
            if (raw) { if (!joystick_ || binding.code < 0 || binding.code >= SDL_JoystickNumAxes(joystick_)) return false; value = SDL_JoystickGetAxis(joystick_, binding.code); }
            else { if (binding.code < 0 || binding.code >= SDL_CONTROLLER_AXIS_MAX) return false; value = SDL_GameControllerGetAxis(controller_, static_cast<SDL_GameControllerAxis>(binding.code)); }
            return binding.direction > 0 ? value >= kAxisThreshold : value <= -kAxisThreshold;
        }
        case BindingKind::ControllerHat:
            return joystick_ && binding.code >= 0 && binding.code < SDL_JoystickNumHats(joystick_) &&
                   (SDL_JoystickGetHat(joystick_, binding.code) & binding.direction) != 0;
        default: return false;
    }
}

u16 InputManager::PressedMask(bool keyboard_enabled) const {
    u16 pressed = 0;
    for (unsigned i = 0; i < kGbaButtonCount; ++i) {
        const auto button = static_cast<GbaButton>(i);
        bool down = keyboard_enabled && BindingPressed(config_.KeyboardBinding(button));
        if (selected_device_ >= 0) down = down || BindingPressed(config_.ControllerBinding(SelectedGuid(), button));
        if (down) pressed |= GbaButtonBit(button);
    }
    return pressed;
}

void InputManager::BeginRebind(GbaButton button, bool controller) {
    rebind_button_ = button;
    rebind_controller_ = controller;
    rebinding_ = true;
}
void InputManager::CancelRebind() { rebinding_ = false; }
void InputManager::ClearBinding(GbaButton button, bool controller) {
    if (controller) config_.SetControllerBinding(SelectedGuid(), button, {});
    else config_.SetKeyboardBinding(button, {});
    Save();
}
void InputManager::Save() { if (!config_path_.empty()) config_.Save(config_path_); }
