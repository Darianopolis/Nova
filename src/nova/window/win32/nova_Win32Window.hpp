#pragma once

#include <nova/window/nova_Window.hpp>

#include <nova/core/nova_Debug.hpp>

#include <nova/core/win32/nova_Win32Include.hpp>

namespace nova
{
    constexpr LPCWSTR Win32WndClassName = L"nova-window-class";

    template<>
    struct Handle<Application>::Impl
    {
        HMODULE module;

        std::vector<Window> windows;

        std::vector<Callback> callbacks;

        bool      running = true;
        bool trace_events = false;

        char16_t high_surrogate;

        std::vector<u32>              key_win_to_nova;
        std::vector<u32>              key_nova_to_win;
        std::vector<std::string_view> key_nova_to_str;

        void InitMappings();

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

        struct Restore {
            Rect2I rect;
        } restore;

        static
        LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    };
}