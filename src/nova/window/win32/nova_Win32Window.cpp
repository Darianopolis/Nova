#include "nova_Win32Window.hpp"

#include <nova/core/win32/nova_Win32.hpp>

#include <windowsx.h>
#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")

namespace nova
{
    LRESULT CALLBACK Window::Impl::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        Window window;
        if (msg == WM_NCCREATE) {
            auto pcreate = reinterpret_cast<CREATESTRUCTW*>(lparam);
            window = std::bit_cast<Window>(pcreate->lpCreateParams);
            window->handle = hwnd;
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, std::bit_cast<LONG_PTR>(window));
        } else {
            window = std::bit_cast<Window>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (!window) {
            return ::DefWindowProcW(hwnd, msg, wparam, lparam);
        }

        // {
        //     auto str = Win32_WindowMessageToString(msg);
        //     if (str.size()) {
        //         Log("MSG: {} ({:#x} :: {}), WPARAM: {} ({:#x}), LPARAM: {} ({:#x})", msg, msg, str, wparam, wparam, lparam, lparam);
        //     }
        // }

        switch (msg) {
            break;case WM_CLOSE:
                window->app->Send({
                    .window = window,
                    .type = EventType::WindowCloseRequested,
                });
            break;case WM_DESTROY:
                window->app->Send({
                    .window = window,
                    .type = EventType::WindowClosing,
                });
                window->handle = {};
                return 0;
            break;case WM_NCDESTROY:
                window->app->RemoveWindow(window);
                delete window.impl;
                return 0;
            break;case WM_LBUTTONDOWN:
                  case WM_LBUTTONUP:
                  case WM_MBUTTONDOWN:
                  case WM_MBUTTONUP:
                  case WM_RBUTTONDOWN:
                  case WM_RBUTTONUP:

                  case WM_KEYDOWN:
                  case WM_KEYUP:

                  case WM_SYSKEYDOWN:
                  case WM_SYSKEYUP:
                {
                    u32 code;
                    u32 repeat;
                    bool pressed;

                    switch (msg) {
                        break;case WM_LBUTTONDOWN: code = VK_LBUTTON;  pressed = true;  repeat = 1;
                        break;case WM_LBUTTONUP:   code = VK_LBUTTON;  pressed = false; repeat = 1;
                        break;case WM_MBUTTONDOWN: code = VK_MBUTTON;  pressed = true;  repeat = 1;
                        break;case WM_MBUTTONUP:   code = VK_MBUTTON;  pressed = false; repeat = 1;
                        break;case WM_RBUTTONDOWN: code = VK_RBUTTON;  pressed = true;  repeat = 1;
                        break;case WM_RBUTTONUP:   code = VK_RBUTTON;  pressed = false; repeat = 1;
                        break;case WM_KEYDOWN:     code = u32(wparam); pressed = true;  repeat = lparam & 0xFFFF;
                        break;case WM_KEYUP:       code = u32(wparam); pressed = false; repeat = lparam & 0xFFFF;
                        break;case WM_SYSKEYDOWN:  code = u32(wparam); pressed = true;  repeat = lparam & 0xFFFF;
                        break;case WM_SYSKEYUP:    code = u32(wparam); pressed = false; repeat = lparam & 0xFFFF;
                        break;default: std::unreachable();
                    }

                    for (u32 i = 0; i < repeat; ++i) {
                        window->app->Send({
                            .window = window,
                            .type = EventType::Input,
                            .input = {
                                .channel = {
                                    .code = code,
                                },
                                .pressed = pressed,
                                .value = pressed ? 1.f : 0.f,
                            }
                        });
                    }

                    return 0;
                }
            break;case WM_MOUSEMOVE:
                {
                    auto x = GET_X_LPARAM(lparam);
                    auto y = GET_Y_LPARAM(lparam);
                    window->app->Send({
                        .window = window,
                        .type = EventType::MouseMove,
                        .mouse_move = {
                            .position = { f32(x), f32(y) },
                        }
                    });

                    return 0;
                }
            break;case WM_MOUSEWHEEL:
                  case WM_MOUSEHWHEEL:
                {
                    auto delta = f32(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
                    window->app->Send({
                        .window = window,
                        .type = EventType::MouseScroll,
                        .scroll = {
                            .scrolled = msg == WM_MOUSEWHEEL
                                ? Vec2 { 0, delta }
                                : Vec2 { delta, 0 }
                        }
                    });

                    return 0;
                }
            break;case WM_CHAR:
                {
                    char16_t codeunit = char16_t(wparam);

                    TextEvent event;

                    if (IS_HIGH_SURROGATE(codeunit)) {
                        window->app->win32_input.high_surrogate = codeunit;
                        return 0;
                    } else if (IS_LOW_SURROGATE(codeunit)) {
                        auto len = simdutf::convert_utf16_to_utf8(std::array{ window->app->win32_input.high_surrogate, codeunit }.data(), 2, event.text);
                        event.text[len] = '\0';
                    } else {
                        auto len = simdutf::convert_utf16_to_utf8(&codeunit, 1, event.text);
                        event.text[len] = '\0';
                    }

                    u32 repeat = lparam & 0xFFFF;

                    for (u32 i = 0; i < repeat; ++i) {
                        window->app->Send({
                            .window = window,
                            .type = EventType::Text,
                            .text = event,
                        });
                    }

                    return 0;
                }
            break;case WM_SETFOCUS:
                  case WM_KILLFOCUS:
                {
                    window->app->Send({
                        .window = window,
                        .type = EventType::WindowFocus,
                        .focus = {
                            .losing  = msg == WM_KILLFOCUS ? window : HWindow{},
                            .gaining = msg == WM_SETFOCUS  ? window : HWindow{},
                        }
                    });

                    return 0;
                }
            break;case WM_ERASEBKGND:
                if (window->bg_brush) {
                    HDC hdc = (HDC)wparam;
                    RECT rect;
                    GetClientRect(hwnd, &rect);
                    FillRect(hdc, &rect, window->bg_brush);

                    return 1;
                }
        }

        return ::DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    Window Window::Create(nova::Application app)
    {
        auto impl = new Impl;
        NOVA_CLEANUP_ON_EXCEPTION(&) { delete impl; };

        impl->app = app;

        app->windows.push_back({ impl });

        {
            DWORD ex_style = WS_EX_APPWINDOW;
            DWORD style = WS_OVERLAPPEDWINDOW;

            impl->handle = ::CreateWindowExW(
                ex_style,
                Win32WndClassName,
                ToUtf16(impl->title).c_str(),
                style,
                CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, CW_USEDEFAULT,
                nullptr, nullptr,
                app->module,
                impl);

            if (!impl->handle) {
                auto res = ::GetLastError();
                NOVA_THROW("Error creating window: {:#x} ({})", u32(HRESULT_FROM_WIN32(res)), win::HResultToString(HRESULT_FROM_WIN32(res)));
            }
        }

        return { impl };
    }

    void Window::Destroy()
    {
        if (!impl) return;

        ::DestroyWindow(impl->handle);

        impl = nullptr;
    }

    Application Window::Application() const
    {
        return impl->app;
    }

    void* Window::NativeHandle() const
    {
        return impl->handle;
    }

    Window Window::Show(bool state) const
    {
        ::ShowWindow(impl->handle, state ? SW_SHOWNA : SW_HIDE);
        if (state) {
            // TODO: Only do if "focus on show"
            Focus();
        }

        return *this;
    }

    void Window::Focus() const
    {
        ::BringWindowToTop(impl->handle);
        ::SetForegroundWindow(impl->handle);
        ::SetFocus(impl->handle);
    }

    Vec2U Window::Size(WindowPart part) const
    {
        if (part == WindowPart::Window) {
            RECT rect;
            ::GetWindowRect(impl->handle, &rect);
            return{ u32(rect.right - rect.left), u32(rect.bottom - rect.top) };
        } else {
            RECT rect;
            ::GetClientRect(impl->handle, &rect);
            return{ u32(rect.right), u32(rect.bottom) };
        }
    }

    Window Window::SetSize(Vec2U new_size, WindowPart part) const
    {
        impl->size = new_size;
        if (impl->swapchain_handles_move_size) return *this;

        if (part == WindowPart::Client) {
            RECT client;
            ::GetClientRect(impl->handle, &client);

            RECT window;
            ::GetWindowRect(impl->handle, &window);

            new_size.x += (window.right - window.left) - client.right;
            new_size.y += (window.bottom - window.top) - client.bottom;
        }

        ::SetWindowPos(impl->handle, 0, 0, 0,
            i32(new_size.x), i32(new_size.y),
            SWP_NOMOVE | SWP_NOZORDER);

        return *this;
    }

    Vec2I Window::Position(WindowPart part) const
    {
        if (part == WindowPart::Window) {
            RECT rect;
            ::GetWindowRect(impl->handle, &rect);
            return{ rect.left, rect.top };
        } else {
            POINT point = {};
            ::ClientToScreen(impl->handle, &point);
            return{ point.x, point.y };
        }
    }

    Window Window::SetPosition(Vec2I pos, WindowPart part) const
    {
        impl->position = pos;
        if (impl->swapchain_handles_move_size) return *this;

        if (part == WindowPart::Client) {
            RECT window_rect;
            ::GetWindowRect(impl->handle, &window_rect);

            POINT client_point = {};
            ::ClientToScreen(impl->handle, &client_point);

            pos.x += window_rect.left - client_point.x;
            pos.y += window_rect.top - client_point.y;
        }

        ::SetWindowPos(impl->handle, 0, pos.x, pos.y, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER);

        return *this;
    }

    Window Window::SetCursor(nova::Cursor cursor) const
    {
        // TODO: Track previous cursor, and update when entering/exiting windows
        // TODO: Handle transparency?

        LPWSTR resource;
        switch (cursor) {
            // TODO: Store and restore previous cursor
            break;case Cursor::Reset:      resource = IDC_ARROW;

            // TODO: Temporary, handle hiding/restoring cursor
            break;case Cursor::None:       resource = IDC_APPSTARTING;

            break;case Cursor::Arrow:      resource = IDC_ARROW;
            break;case Cursor::IBeam:      resource = IDC_IBEAM;
            break;case Cursor::Hand:       resource = IDC_HAND;
            break;case Cursor::ResizeNS:   resource = IDC_SIZENS;
            break;case Cursor::ResizeEW:   resource = IDC_SIZEWE;
            break;case Cursor::ResizeNWSE: resource = IDC_SIZENWSE;
            break;case Cursor::ResizeNESW: resource = IDC_SIZENESW;
            break;case Cursor::ResizeAll:  resource = IDC_SIZEALL;
            break;case Cursor::NotAllowed: resource = IDC_NO;

            break;default: std::unreachable();
        }

        // TODO: Preload cursors and keep handles around
        auto handle = ::LoadCursorW(nullptr, resource);

        ::SetCursor(handle);

        return *this;
    }

    bool Window::Minimized() const
    {
        return IsIconic(impl->handle);
    }

    Window Window::SetTitle(std::string _title) const
    {
        impl->title = std::move(_title);

        ::SetWindowTextW(impl->handle, ToUtf16(impl->title).c_str());

        return *this;
    }

    StringView Window::Title() const
    {
        return impl->title;
    }

    Window Window::SetDarkMode(bool state) const
    {
        BOOL use_dark_mode = state;
        ::DwmSetWindowAttribute(impl->handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &use_dark_mode, sizeof(use_dark_mode));

        return *this;
    }


    void Win32_UpdateStyles(HWND hwnd, u32 add, u32 remove, u32 add_ext, u32 remove_ext)
    {
        auto old = (u32)::GetWindowLongPtrW(hwnd, GWL_STYLE);
        ::SetWindowLongPtrW(hwnd, GWL_STYLE, LONG_PTR((old & ~remove) | add));

        auto old_ext = (u32)::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        ::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, LONG_PTR((old_ext & ~remove_ext) | add_ext));
    }

    Window Window::SetDecorate(bool state) const
    {
        if (state) {
            Win32_UpdateStyles(impl->handle, WS_OVERLAPPEDWINDOW, 0, 0, 0);
        } else {
            Win32_UpdateStyles(impl->handle, 0, WS_OVERLAPPEDWINDOW, 0, 0);
        }

        return *this;
    }

    Window Window::SetTransparency(TransparencyMode mode, Vec3U chroma_key, f32 alpha) const
    {
        switch (mode) {
            break;case TransparencyMode::Disabled:
                Win32_UpdateStyles(impl->handle, 0, 0, 0, WS_EX_LAYERED | WS_EX_TRANSPARENT);
            break;case TransparencyMode::ChromaKey:
                Win32_UpdateStyles(impl->handle, 0, 0, WS_EX_LAYERED | WS_EX_TRANSPARENT, 0);
                ::SetLayeredWindowAttributes(impl->handle, RGB(chroma_key.r, chroma_key.g, chroma_key.b), 0, LWA_COLORKEY);
            break;case TransparencyMode::Static:
                Win32_UpdateStyles(impl->handle, 0, 0, WS_EX_LAYERED | WS_EX_TRANSPARENT, 0);
                ::SetLayeredWindowAttributes(impl->handle, 0, BYTE(alpha * 255.f), LWA_ALPHA);
            break;case TransparencyMode::PerPixel:
                // TODO: This should communicate with swapchain
                // Must clear layered bit in case last state was ChromaKey
                Win32_UpdateStyles(impl->handle, 0, 0, 0, WS_EX_LAYERED | WS_EX_TRANSPARENT);
                Win32_UpdateStyles(impl->handle, 0, 0, WS_EX_LAYERED, 0);
        }

        // // Make window transparent
        // bool transparentClientArea = mode != TransparencyMode::Disabled;

        // HRGN region = nullptr;
        // if (transparentClientArea)
        //     region = CreateRectRgn(0, 0, -1, -1);
        // DWM_BLURBEHIND bb{};
        // bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        // bb.hRgnBlur = region;
        // bb.fEnable = transparentClientArea;
        // bb.fTransitionOnMaximized = false;
        // if (FAILED(DwmEnableBlurBehindWindow(impl->handle, &bb))) {
        //     throw std::runtime_error("Failed to make window transparent!");
        // }
        // DeleteObject(region);

        // DWM_SYSTEMBACKDROP_TYPE attribute = DWMSBT_NONE;
        // // if (translucency >= OsWindowTranslucency::BackgroundBlur) {
        //     // attribute = DWMSBT_TABBEDWINDOW;
        // // } else if (translucency >= OsWindowTranslucency::TransientBlur) {
        //     // attribute = DWMSBT_TRANSIENTWINDOW;
        // // }
        // DwmSetWindowAttribute(impl->handle, DWMWA_SYSTEMBACKDROP_TYPE, &attribute, sizeof(attribute));

        return *this;
    }

    Window Window::SetBackgroundColor(bool enabled, Vec3 color) const
    {
        if (impl->bg_brush) {
            DeleteObject(impl->bg_brush);
        }

        if (enabled) {
            impl->bg_brush = ::CreateSolidBrush(RGB(BYTE(color.r * 255.f), BYTE(color.g * 255.f), BYTE(color.b * 255.f)));
        }

        RECT client_rect;
        ::GetClientRect(impl->handle, &client_rect);
        ::InvalidateRect(impl->handle, &client_rect, true);

        return *this;
    }

    Window Window::SetTopmost(bool topmost) const
    {
        if (topmost) {
            ::SetWindowPos(impl->handle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        } else {
            auto exstyle = (u32)::GetWindowLongPtrW(impl->handle, GWL_EXSTYLE);
            if (exstyle & WS_EX_TOPMOST) {
                ::SetWindowPos(impl->handle, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
        }

        return *this;
    }

    Window Window::SetAppWindow(bool appwindow) const
    {
        if (appwindow) {
            Win32_UpdateStyles(impl->handle, 0, 0, WS_EX_APPWINDOW, WS_EX_TOOLWINDOW);
        } else {
            Win32_UpdateStyles(impl->handle, 0, 0, WS_EX_TOOLWINDOW, WS_EX_APPWINDOW);
        }

        return *this;
    }

    Window Window::SetFullscreen(bool enabled) const
    {
        if (enabled) {
            if (impl->restore.stored) return *this;

            {
                RECT rect;
                ::GetWindowRect(impl->handle, &rect);
                impl->restore.rect = {
                    .offset = { rect.left, rect.top },
                    .extent = { rect.right - rect.left, rect.bottom - rect.top }
                };
            }

            impl->restore.stored = true;

            Log("Restore: {}", glm::to_string(impl->restore.rect.offset));

            // TODO: Pick monitor based on window location
            auto monitor = impl->app.PrimaryDisplay();

            Log("Fullscreening to: {} @ {}", glm::to_string(monitor.Size()), glm::to_string(monitor.Position()));

            SetDecorate(false);
            SetPosition(monitor.Position(), WindowPart::Window);
            // SetPosition(monitor.Position() + Vec2I(0, 0), WindowPart::Window);
            SetSize(monitor.Size(), WindowPart::Window);
            // SetSize(monitor.Size() - Vec2I(0, 1), WindowPart::Window);
            // ::ShowWindow(impl->handle, SW_MAXIMIZE);
            ::ShowWindow(impl->handle, SW_NORMAL);

        } else if (impl->restore.stored) {
            Log("Restoring to: {}", glm::to_string(impl->restore.rect.offset));

            impl->restore.stored = false;

            SetDecorate(true);
            ::ShowWindow(impl->handle, SW_NORMAL);
            SetPosition(impl->restore.rect.offset, WindowPart::Window);
            SetSize(impl->restore.rect.extent, WindowPart::Window);
        }

        return *this;
    }
}