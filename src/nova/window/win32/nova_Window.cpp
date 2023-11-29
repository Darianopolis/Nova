#include "nova_Win32Window.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Stack.hpp>

namespace nova
{
    LRESULT CALLBACK Window::Impl::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
NOVA_DEBUG();
        Window window;
NOVA_DEBUG();
        if (msg == WM_NCCREATE) {
NOVA_DEBUG();
            auto pcreate = reinterpret_cast<CREATESTRUCTW*>(lparam);
NOVA_DEBUG();
            window = std::bit_cast<Window>(pcreate->lpCreateParams);
NOVA_DEBUG();
            window->handle = hwnd;
NOVA_DEBUG();
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, std::bit_cast<LONG_PTR>(window));
NOVA_DEBUG();
        } else {
NOVA_DEBUG();
            window = std::bit_cast<Window>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
NOVA_DEBUG();
        }

NOVA_DEBUG();
        if (!window) {
NOVA_DEBUG();
            return DefWindowProcW(hwnd, msg, wparam, lparam);
        }

NOVA_DEBUG();
        switch (msg) {
            break;case WM_DESTROY:
NOVA_DEBUG();
                NOVA_LOG("Destroying window...");
                return 0;
NOVA_DEBUG();
            break;case WM_NCDESTROY:
NOVA_DEBUG();
                NOVA_LOG("Window destroyed, freeing handle");
NOVA_DEBUG();
                delete window.impl;
NOVA_DEBUG();
                PostQuitMessage(0);
NOVA_DEBUG();
                return 0;
NOVA_DEBUG();
        }
NOVA_DEBUG();

        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    Window Window::Create(Application app, const WindowInfo& info)
    {
        auto impl = new Impl;
        NOVA_DEFER(&) { if (exceptions) delete impl; };

        impl->app = app;

        {
            NOVA_STACK_POINT();

            DWORD ex_style = 0;
            DWORD style = WS_OVERLAPPEDWINDOW;

            int cx = CW_USEDEFAULT, cy = CW_USEDEFAULT;
            if (info.size.x && info.size.y) {
                cx = int(info.size.x);
                cy = int(info.size.y);
            }

NOVA_DEBUG();
            impl->handle = CreateWindowExW(
                ex_style,
                Win32WndClassName,
                NOVA_STACK_TO_UTF16(info.title).data(),
                style,
                CW_USEDEFAULT, CW_USEDEFAULT,
                cx, cy,
                nullptr, nullptr,
                app->module,
                impl);
NOVA_DEBUG();

            if (!impl->handle) {
                auto res = GetLastError();
                detail::DebugRes(res);
                NOVA_THROW("Error creating window: {:#x}", u32(HRESULT_FROM_WIN32(res)));
            }

            ShowWindow(impl->handle, SW_SHOW);
        }

        return { impl };
    }

    void Window::Destroy()
    {
        if (!impl) return;

        // TODO
        DestroyWindow(impl->handle);

        delete impl;
        impl = nullptr;
    }

    Vec2U Window::GetSize() const
    {
        RECT rect;
        GetClientRect(impl->handle, &rect);
        return { rect.right - rect.left, rect.bottom - rect.top };
    }

    void Window::SetSize(Vec2U new_size) const
    {
        SetWindowPos(impl->handle, nullptr, 0, 0, i32(new_size.x), i32(new_size.y),
            SWP_NOMOVE | SWP_NOOWNERZORDER);
    }

    void Window::SetFullscreen(bool enabled) const
    {
        // TODO
    }
}