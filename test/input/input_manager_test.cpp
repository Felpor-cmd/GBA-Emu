#include <catch2/catch_test_macros.hpp>
#include <SDL2/SDL.h>
#include <filesystem>
#include "input/input_manager.hpp"

namespace {
struct SdlJoystickFixture {
    SdlJoystickFixture() { SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER); }
    ~SdlJoystickFixture() {
        if (device >= 0) SDL_JoystickDetachVirtual(device);
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    }
    int device = -1;
};
}

TEST_CASE("Input manager selects a connected controller and polls its buttons") {
    SdlJoystickFixture sdl;
    sdl.device = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 10, 1);
    REQUIRE(sdl.device >= 0);
    const auto path = std::filesystem::temp_directory_path() / "gba-input-manager-test.ini";
    std::filesystem::remove(path);
    {
        InputManager input(path.string());
        REQUIRE(input.Initialize());
        REQUIRE(input.Devices().size() == 1);
        REQUIRE(input.SelectedDevice() == 0);
        SDL_Joystick* joystick = SDL_JoystickOpen(sdl.device);
        REQUIRE(joystick != nullptr);
        REQUIRE(SDL_JoystickSetVirtualButton(joystick, 0, 1) == 0);
        REQUIRE(SDL_JoystickSetVirtualHat(joystick, 0, SDL_HAT_UP) == 0);
        SDL_JoystickUpdate();
        REQUIRE((input.PressedMask(false) & GbaButtonBit(GbaButton::A)) != 0);
        REQUIRE((input.PressedMask(false) & GbaButtonBit(GbaButton::B)) == 0);
        REQUIRE((input.PressedMask(false) & GbaButtonBit(GbaButton::Up)) != 0);
        input.Config().SetControllerBinding(input.Config().SelectedControllerGuid(), GbaButton::R,
            {BindingKind::ControllerAxis, 0, 1, true});
        REQUIRE(SDL_JoystickSetVirtualAxis(joystick, 0, 20000) == 0);
        SDL_JoystickUpdate();
        REQUIRE((input.PressedMask(false) & GbaButtonBit(GbaButton::R)) != 0);
        SDL_JoystickClose(joystick);
    }
    std::filesystem::remove(path);
}

TEST_CASE("Input remapping captures keyboard keys, cancels with Escape, and clears bindings") {
    SdlJoystickFixture sdl;
    InputManager input(std::filesystem::temp_directory_path().append("gba-input-rebind-test.ini").string());
    REQUIRE(input.Initialize());
    input.BeginRebind(GbaButton::A, false);
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.scancode = SDL_SCANCODE_Q;
    input.ProcessEvent(event);
    REQUIRE_FALSE(input.IsRebinding());
    REQUIRE(input.Config().KeyboardBinding(GbaButton::A).code == SDL_SCANCODE_Q);

    input.BeginRebind(GbaButton::A, false);
    event.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
    input.ProcessEvent(event);
    REQUIRE_FALSE(input.IsRebinding());
    REQUIRE(input.Config().KeyboardBinding(GbaButton::A).code == SDL_SCANCODE_Q);
    input.ClearBinding(GbaButton::A, false);
    REQUIRE(input.Config().KeyboardBinding(GbaButton::A).kind == BindingKind::Unbound);
    std::filesystem::remove(std::filesystem::temp_directory_path() / "gba-input-rebind-test.ini");
}

TEST_CASE("Input manager follows controller removal and hot-plug") {
    SdlJoystickFixture sdl;
    sdl.device = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 10, 1);
    REQUIRE(sdl.device >= 0);
    InputManager input(std::filesystem::temp_directory_path().append("gba-input-hotplug-test.ini").string());
    REQUIRE(input.Initialize());
    REQUIRE(input.Devices().size() == 1);
    const SDL_JoystickID old_instance = input.Devices()[0].instance;
    REQUIRE(SDL_JoystickDetachVirtual(sdl.device) == 0);
    SDL_Event removed{};
    removed.type = SDL_JOYDEVICEREMOVED;
    removed.jdevice.which = old_instance;
    input.ProcessEvent(removed);
    REQUIRE(input.Devices().empty());
    REQUIRE(input.SelectedDevice() == -1);

    sdl.device = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 10, 1);
    REQUIRE(sdl.device >= 0);
    SDL_Event added{};
    added.type = SDL_JOYDEVICEADDED;
    added.jdevice.which = sdl.device;
    input.ProcessEvent(added);
    REQUIRE(input.Devices().size() == 1);
    REQUIRE(input.SelectedDevice() == 0);
    std::filesystem::remove(std::filesystem::temp_directory_path() / "gba-input-hotplug-test.ini");
}
