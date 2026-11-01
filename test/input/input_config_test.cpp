#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <SDL2/SDL_scancode.h>
#include "input/input_config.hpp"

TEST_CASE("Keyboard defaults use WASD, J/K, U/I, Enter, and Right Shift") {
    InputConfig config;
    REQUIRE(config.KeyboardBinding(GbaButton::Up).code == SDL_SCANCODE_W);
    REQUIRE(config.KeyboardBinding(GbaButton::Down).code == SDL_SCANCODE_S);
    REQUIRE(config.KeyboardBinding(GbaButton::Left).code == SDL_SCANCODE_A);
    REQUIRE(config.KeyboardBinding(GbaButton::Right).code == SDL_SCANCODE_D);
    REQUIRE(config.KeyboardBinding(GbaButton::A).code == SDL_SCANCODE_J);
    REQUIRE(config.KeyboardBinding(GbaButton::B).code == SDL_SCANCODE_K);
    REQUIRE(config.KeyboardBinding(GbaButton::L).code == SDL_SCANCODE_U);
    REQUIRE(config.KeyboardBinding(GbaButton::R).code == SDL_SCANCODE_I);
    REQUIRE(config.KeyboardBinding(GbaButton::Start).code == SDL_SCANCODE_RETURN);
    REQUIRE(config.KeyboardBinding(GbaButton::Select).code == SDL_SCANCODE_RSHIFT);
}

TEST_CASE("Input mappings persist keyboard, per-controller bindings, and selected device") {
    const auto path = std::filesystem::temp_directory_path() / "gba-input-config-test.ini";
    std::filesystem::remove(path);
    InputConfig source;
    source.SetKeyboardBinding(GbaButton::A, {BindingKind::KeyboardKey, SDL_SCANCODE_Q, 0, false});
    source.SetControllerBinding("device-guid", GbaButton::R,
                                {BindingKind::ControllerAxis, 3, -1, true});
    source.SetSelectedControllerGuid("device-guid");
    REQUIRE(source.Save(path.string()));

    InputConfig loaded;
    REQUIRE(loaded.Load(path.string()));
    REQUIRE(loaded.KeyboardBinding(GbaButton::A).kind == BindingKind::KeyboardKey);
    REQUIRE(loaded.KeyboardBinding(GbaButton::A).code == SDL_SCANCODE_Q);
    REQUIRE(loaded.ControllerBinding("device-guid", GbaButton::R).kind == BindingKind::ControllerAxis);
    REQUIRE(loaded.ControllerBinding("device-guid", GbaButton::R).code == 3);
    REQUIRE(loaded.ControllerBinding("device-guid", GbaButton::R).direction == -1);
    REQUIRE(loaded.ControllerBinding("device-guid", GbaButton::R).raw);
    REQUIRE(loaded.SelectedControllerGuid() == "device-guid");
    std::filesystem::remove(path);
}

TEST_CASE("Unknown input config versions leave default mappings intact") {
    const auto path = std::filesystem::temp_directory_path() / "gba-input-config-invalid-test.ini";
    {
        std::ofstream output(path);
        output << "gba-input-config-v99\n";
    }
    InputConfig config;
    REQUIRE_FALSE(config.Load(path.string()));
    REQUIRE(config.KeyboardBinding(GbaButton::A).code == SDL_SCANCODE_J);
    std::filesystem::remove(path);
}
