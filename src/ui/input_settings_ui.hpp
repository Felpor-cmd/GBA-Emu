#pragma once

#include "../input/input_manager.hpp"
#include <SDL2/SDL.h>

struct ImGuiContext;

class InputSettingsUi {
public:
    bool Initialize(SDL_Window* window, SDL_Renderer* renderer);
    void Shutdown();
    void ProcessEvent(const SDL_Event& event);
    void ToggleMenu() { menu_open_ = !menu_open_; }
    bool HasOpenPanel() const { return menu_open_ || settings_open_; }
    void Render(InputManager& input, SDL_Window* game_window, SDL_Renderer* renderer);

private:
    void DrawMenu();
    void DrawSettings(InputManager& input);
    bool OpenSettingsWindow();
    void CloseSettingsWindow();
    static Uint32 EventWindowId(const SDL_Event& event);
    bool initialized_ = false;
    bool menu_open_ = false;
    bool settings_open_ = false;
    SDL_Window* game_window_ = nullptr;
    SDL_Renderer* game_renderer_ = nullptr;
    SDL_Window* settings_window_ = nullptr;
    SDL_Renderer* settings_renderer_ = nullptr;
    ImGuiContext* game_context_ = nullptr;
    ImGuiContext* settings_context_ = nullptr;
};
