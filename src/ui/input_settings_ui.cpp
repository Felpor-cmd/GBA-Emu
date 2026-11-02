#include "input_settings_ui.hpp"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

namespace {
constexpr GbaButton kButtons[] = {GbaButton::A, GbaButton::B, GbaButton::Select, GbaButton::Start,
    GbaButton::Right, GbaButton::Left, GbaButton::Up, GbaButton::Down, GbaButton::R, GbaButton::L};
constexpr const char* kButtonNames[] = {"A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L"};

const char* KeyName(const InputBinding& binding) {
    if (binding.kind == BindingKind::Unbound) return "Unbound";
    if (binding.kind != BindingKind::KeyboardKey) return "Other binding";
    return SDL_GetScancodeName(static_cast<SDL_Scancode>(binding.code));
}

std::string ControllerBindingName(const InputBinding& binding) {
    if (binding.kind == BindingKind::Unbound) return "Unbound";
    if (binding.kind == BindingKind::ControllerButton && !binding.raw) {
        const char* name = SDL_GameControllerGetStringForButton(static_cast<SDL_GameControllerButton>(binding.code));
        return name ? name : "Button";
    }
    if (binding.kind == BindingKind::ControllerButton) return "Button " + std::to_string(binding.code);
    if (binding.kind == BindingKind::ControllerAxis)
        return "Axis " + std::to_string(binding.code) + (binding.direction > 0 ? " +" : " -");
    if (binding.kind == BindingKind::ControllerHat) return "Hat " + std::to_string(binding.code);
    return "Unbound";
}

void ApplyStyle() {
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 7.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 5.0f;
    style.WindowBorderSize = 1.0f;
    auto* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.035f, 0.045f, 0.98f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.055f, 0.055f, 0.065f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.08f, 0.10f, 0.8f);
    colors[ImGuiCol_Button] = ImVec4(0.36f, 0.055f, 0.075f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.60f, 0.08f, 0.10f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.78f, 0.10f, 0.13f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.40f, 0.045f, 0.075f, 0.85f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.62f, 0.075f, 0.10f, 0.95f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.075f, 0.09f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.93f, 0.17f, 0.20f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.12f, 0.16f, 1.0f);
}
}

bool InputSettingsUi::Initialize(SDL_Window* window, SDL_Renderer* renderer) {
    IMGUI_CHECKVERSION();
    game_window_ = window;
    game_renderer_ = renderer;
    game_context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ApplyStyle();
    if (!ImGui_ImplSDL2_InitForSDLRenderer(window, renderer) || !ImGui_ImplSDLRenderer2_Init(renderer)) {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    initialized_ = true;
    return true;
}

void InputSettingsUi::Shutdown() {
    if (!initialized_) return;
    CloseSettingsWindow();
    ImGui::SetCurrentContext(game_context_);
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(game_context_);
    game_context_ = nullptr;
    initialized_ = false;
}

Uint32 InputSettingsUi::EventWindowId(const SDL_Event& event) {
    switch (event.type) {
        case SDL_KEYDOWN: case SDL_KEYUP: return event.key.windowID;
        case SDL_TEXTEDITING: return event.edit.windowID;
        case SDL_TEXTINPUT: return event.text.windowID;
        case SDL_MOUSEMOTION: return event.motion.windowID;
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: return event.button.windowID;
        case SDL_MOUSEWHEEL: return event.wheel.windowID;
        case SDL_WINDOWEVENT: return event.window.windowID;
        default: return 0;
    }
}

void InputSettingsUi::ProcessEvent(const SDL_Event& event) {
    if (!initialized_) return;
    const Uint32 window_id = EventWindowId(event);
    if (settings_context_ && window_id == SDL_GetWindowID(settings_window_)) {
        ImGui::SetCurrentContext(settings_context_);
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
            CloseSettingsWindow();
    } else {
        ImGui::SetCurrentContext(game_context_);
        ImGui_ImplSDL2_ProcessEvent(&event);
    }
}

bool InputSettingsUi::OpenSettingsWindow() {
    if (settings_context_) return true;
    settings_window_ = SDL_CreateWindow("Input Configuration", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, 620, 560, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!settings_window_) return false;
    settings_renderer_ = SDL_CreateRenderer(settings_window_, -1, SDL_RENDERER_ACCELERATED);
    if (!settings_renderer_) settings_renderer_ = SDL_CreateRenderer(settings_window_, -1, SDL_RENDERER_SOFTWARE);
    if (!settings_renderer_) { SDL_DestroyWindow(settings_window_); settings_window_ = nullptr; return false; }
    settings_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(settings_context_);
    ImGui::GetIO().IniFilename = nullptr;
    ApplyStyle();
    if (!ImGui_ImplSDL2_InitForSDLRenderer(settings_window_, settings_renderer_) ||
        !ImGui_ImplSDLRenderer2_Init(settings_renderer_)) {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext(settings_context_);
        settings_context_ = nullptr;
        SDL_DestroyRenderer(settings_renderer_);
        SDL_DestroyWindow(settings_window_);
        settings_renderer_ = nullptr;
        settings_window_ = nullptr;
        ImGui::SetCurrentContext(game_context_);
        return false;
    }
    settings_open_ = true;
    ImGui::SetCurrentContext(game_context_);
    return true;
}

void InputSettingsUi::CloseSettingsWindow() {
    if (settings_context_) {
        ImGui::SetCurrentContext(settings_context_);
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext(settings_context_);
        settings_context_ = nullptr;
    }
    SDL_DestroyRenderer(settings_renderer_);
    SDL_DestroyWindow(settings_window_);
    settings_renderer_ = nullptr;
    settings_window_ = nullptr;
    settings_open_ = false;
    if (game_context_) ImGui::SetCurrentContext(game_context_);
}

void InputSettingsUi::DrawMenu() {
    if (!menu_open_) return;
    ImGui::SetNextWindowPos(ImVec2(18, 18), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.96f);
    if (ImGui::Begin("GBA Emulator", &menu_open_, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextColored(ImVec4(0.94f, 0.16f, 0.19f, 1.0f), "EMULATOR MENU");
        ImGui::Separator();
        if (ImGui::Button("Input Configuration", ImVec2(220, 36))) {
            OpenSettingsWindow();
            menu_open_ = false;
        }
        ImGui::TextDisabled("F1  Close menu");
    }
    ImGui::End();
}

void InputSettingsUi::DrawSettings(InputManager& input) {
    if (!settings_context_) return;
    ImGui::TextColored(ImVec4(0.94f, 0.16f, 0.19f, 1.0f), "CONTROLS");
    ImGui::SameLine(); ImGui::TextDisabled("Keyboard + controller");
    ImGui::Separator();

    const unsigned speed = input.Config().FastForwardSpeed();
    const std::string speed_label = std::to_string(speed) + "x";
    ImGui::SetNextItemWidth(150);
    if (ImGui::BeginCombo("Tab fast-forward speed", speed_label.c_str())) {
        for (const unsigned choice : {1u, 2u, 3u, 5u, 10u}) {
            const std::string label = std::to_string(choice) + "x";
            if (ImGui::Selectable(label.c_str(), choice == speed)) input.SetFastForwardSpeed(choice);
        }
        ImGui::EndCombo();
    }
    ImGui::TextDisabled("Hold Tab to use the selected speed; actual speed depends on the game and computer.");
    ImGui::Separator();

    const auto& devices = input.Devices();
    if (devices.empty()) {
        ImGui::Text("Controller"); ImGui::SameLine(); ImGui::TextDisabled("No controller connected");
    } else {
        const int selected = input.SelectedDevice();
        const char* selected_name = selected >= 0 ? devices[selected].name.c_str() : "None";
        ImGui::SetNextItemWidth(310);
        if (ImGui::BeginCombo("Controller", selected_name)) {
            for (int i = 0; i < static_cast<int>(devices.size()); ++i) {
                const bool active = selected == i;
                const std::string label = devices[i].name + "  ·  " + devices[i].type;
                if (ImGui::Selectable(label.c_str(), active)) input.SelectDevice(i);
                if (active) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (selected >= 0) ImGui::TextDisabled("%s", devices[selected].type.c_str());
    }

    ImGui::Spacing();
    ImGui::TextWrapped("Select a binding, then press a key or controller button. Move an axis to bind it. Escape cancels.");
    if (ImGui::BeginTable("bindings", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("GBA button", ImGuiTableColumnFlags_WidthFixed, 105);
        ImGui::TableSetupColumn("Keyboard", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Controller", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Clear", ImGuiTableColumnFlags_WidthFixed, 55);
        ImGui::TableHeadersRow();
        for (unsigned row = 0; row < kGbaButtonCount; ++row) {
            const auto button = kButtons[row];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", kButtonNames[row]);
            ImGui::TableSetColumnIndex(1);
            const bool keyboard_listening = input.IsRebinding() && !input.RebindingController() && input.RebindingButton() == button;
            const std::string key = keyboard_listening ? "Press a key…##key" + std::to_string(row) : std::string(KeyName(input.Config().KeyboardBinding(button))) + "##key" + std::to_string(row);
            if (ImGui::Button(key.c_str(), ImVec2(-1, 0))) input.BeginRebind(button, false);
            ImGui::TableSetColumnIndex(2);
            const bool controller_listening = input.IsRebinding() && input.RebindingController() && input.RebindingButton() == button;
            const std::string pad_label = controller_listening ? "Press a button…##pad" + std::to_string(row) : ControllerBindingName(input.Config().ControllerBinding(input.Config().SelectedControllerGuid(), button)) + "##pad" + std::to_string(row);
            ImGui::BeginDisabled(devices.empty());
            if (ImGui::Button(pad_label.c_str(), ImVec2(-1, 0))) input.BeginRebind(button, true);
            ImGui::EndDisabled();
            ImGui::TableSetColumnIndex(3);
            if (ImGui::SmallButton(("K##clear" + std::to_string(row)).c_str())) input.ClearBinding(button, false);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear keyboard binding");
            ImGui::SameLine();
            if (ImGui::SmallButton(("P##clearpad" + std::to_string(row)).c_str())) input.ClearBinding(button, true);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear controller binding");
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Keyboard and selected controller inputs are combined. Changes save automatically.");
}

void InputSettingsUi::Render(InputManager& input, SDL_Window* game_window, SDL_Renderer* renderer) {
    if (!initialized_) return;
    ImGui::SetCurrentContext(game_context_);
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    DrawMenu();
    ImGui::Render();
    SDL_RenderSetScale(game_renderer_, 1.0f, 1.0f);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);

    if (settings_context_) {
        ImGui::SetCurrentContext(settings_context_);
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
        ImGui::Begin("Input Configuration", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
        DrawSettings(input);
        ImGui::End();
        ImGui::Render();
        SDL_SetRenderDrawColor(settings_renderer_, 9, 9, 12, 255);
        SDL_RenderClear(settings_renderer_);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), settings_renderer_);
        SDL_RenderPresent(settings_renderer_);
    }
    (void)game_window;
}
