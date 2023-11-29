#include "example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/window/nova_Window.hpp>
#include <nova/core/nova_Debug.hpp>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

NOVA_EXAMPLE(AppTest, "app")
{
NOVA_DEBUG();
    auto app = nova::Application::Create();
NOVA_DEBUG();
    auto window = nova::Window::Create(app, {
        .title = "Application Test",
        .size = { 1920, 1080 },
    });
NOVA_DEBUG();
    app.Run();
NOVA_DEBUG();

    // WNDCLASSW class_info {
    //     .lpfnWndProc = WindowProc,
    //     .cbWndExtra = 8,
    //     .hInstance = GetModuleHandleW(nullptr),
    //     .lpszClassName = L"TestClass",
    // };

    // RegisterClassW(&class_info);

    // auto window = CreateWindowExW(
    //     0,
    //     L"TestClass",
    //     L"Test Window",
    //     WS_OVERLAPPED,
    //     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
    //     nullptr, nullptr,
    //     GetModuleHandleW(nullptr),
    //     nullptr);

    // ShowWindow(window, SW_SHOW);

    // MSG msg;
    // while (GetMessageW(&msg, nullptr, 0, 0)) {
    //     TranslateMessage(&msg);
    //     DispatchMessageW(&msg);
    // }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
NOVA_DEBUG();
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}