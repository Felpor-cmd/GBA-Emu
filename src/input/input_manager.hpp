#pragma once

#include "input_config.hpp"
#include <SDL2/SDL.h>
#include <string>
#include <vector>

struct InputDeviceInfo {
    SDL_JoystickID instance = -1;
    std::string guid;
    std::string name;
    std::string type;
    bool mapped = false;
};

class InputManager {
public:
    explicit InputManager(std::string config_path = {});
    ~InputManager();
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    bool Initialize();
    void Shutdown();
    void ProcessEvent(const SDL_Event& event);
    u16 PressedMask(bool keyboard_enabled) const;
    const std::vector<InputDeviceInfo>& Devices() const { return devices_; }
    int SelectedDevice() const { return selected_device_; }
    void SelectDevice(int index);
    const InputConfig& Config() const { return config_; }
    InputConfig& Config() { return config_; }

    void BeginRebind(GbaButton button, bool controller);
    void CancelRebind();
    void ClearBinding(GbaButton button, bool controller);
    bool IsRebinding() const { return rebinding_; }
    GbaButton RebindingButton() const { return rebind_button_; }
    bool RebindingController() const { return rebind_controller_; }

private:
    void RefreshDevices();
    void OpenSelected();
    void CloseSelected();
    std::string SelectedGuid() const;
    bool BindingPressed(const InputBinding& binding) const;
    bool CaptureBinding(const InputBinding& binding);
    void Save();

    std::string config_path_;
    InputConfig config_;
    std::vector<InputDeviceInfo> devices_;
    int selected_device_ = -1;
    SDL_GameController* controller_ = nullptr;
    SDL_Joystick* joystick_ = nullptr;
    SDL_JoystickID joystick_instance_ = -1;
    bool initialized_ = false;
    bool rebinding_ = false;
    bool rebind_controller_ = false;
    GbaButton rebind_button_ = GbaButton::A;
};
