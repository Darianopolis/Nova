#pragma once

#include <nova/core/win32/nova_Win32.hpp>

#include <GameInput.h>
#include <nova/window/nova_Window.hpp>

namespace nova
{
    constexpr LPCWSTR Win32WndClassName = L"nova-window-class";

    template<>
    struct Handle<Application>::Impl
    {
        HMODULE module;

        std::vector<Window> windows;

        void RemoveWindow(Window window);

        std::vector<AppCallback> callbacks;

        bool      running = true;
        bool trace_events = false;

        struct Win32InputState
        {
            char16_t high_surrogate;

            std::vector<u32> from_win32_virtual_key;
            std::vector<u32>   to_win32_virtual_key;
            std::vector<StringView>       key_names;
        } win32_input;

        void InitMappings();

        struct GameInputState
        {
            struct GameInputDevice
            {
                struct GI_MouseButton
                {
                    u32       id;
                    bool pressed;
                };

                IGameInputDevice*         handle;
                GameInputKind              kinds;

                std::string name;
                std::string manufacturer;

                std::array<bool, 7> mouse_buttons;
                Vec2                  mouse_wheel;
                Vec2                    mouse_pos;

                std::vector<u8> key_states;

                std::vector<f32> axis_states;

                std::vector<u8> button_states;

                std::vector<GameInputSwitchPosition> switch_states;
            };

            IGameInput*                           handle;
            GameInputCallbackToken device_callback_token;

            std::vector<GameInputDevice> devices;
        } game_input;

        void InitGameInput();
        void DestroyGameInput();

        void Send(AppEvent event);
    };

    template<>
    struct Handle<Display>::Impl
    {
        Application app;

        HMONITOR monitor;
    };

    template<>
    struct Handle<Window>::Impl
    {
        Application app;

        HWND handle;

        std::string title;

        struct Restore {
            Rect2I rect;
        } restore;

        static
        LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    };
}