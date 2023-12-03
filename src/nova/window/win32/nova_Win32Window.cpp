#include "nova_Win32Window.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Stack.hpp>

#include <nova/window/win32/nova_Win32Debug.hpp>
#include <nova/core/win32/nova_Win32Utility.hpp>

#include <windowsx.h>

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
        //     auto str = Win32MessageToString(msg);
        //     if (str.size()) {
        //         NOVA_LOG("MSG: {:3} ({:#5x} :: {:15}), WPARAM: {:8} ({:#10x}), LPARAM: {:8} ({:#10x})", msg, msg, str, wparam, wparam, lparam, lparam);
        //     }
        // }

        switch (msg) {
            break;case WM_DESTROY:
                NOVA_LOG("Destroying window...");
                return 0;
            break;case WM_NCDESTROY:
                NOVA_LOG("Window destroyed, freeing handle");
                delete window.impl;
                ::PostQuitMessage(0);
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
                        window->app->high_surrogate = codeunit;
                        return 0;
                    } else if (IS_LOW_SURROGATE(codeunit)) {
                        auto len = simdutf::convert_utf16_to_utf8(std::array{ window->app->high_surrogate, codeunit }.data(), 2, event.text);
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
        }

        return ::DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    Window Window::Create(Application app, const WindowInfo& info)
    {
        auto impl = new Impl;
        NOVA_DEFER(&) { if (exceptions) delete impl; };

        impl->app = app;

        app->windows.push_back({ impl });

        {
            NOVA_STACK_POINT();

            DWORD ex_style = WS_EX_APPWINDOW;
            DWORD style = WS_OVERLAPPEDWINDOW;

            int cx = CW_USEDEFAULT, cy = CW_USEDEFAULT;
            if (info.size.x && info.size.y) {
                cx = int(info.size.x);
                cy = int(info.size.y);
            }

            impl->handle = ::CreateWindowExW(
                ex_style,
                Win32WndClassName,
                NOVA_STACK_TO_UTF16(info.title).data(),
                style,
                CW_USEDEFAULT, CW_USEDEFAULT,
                cx, cy,
                nullptr, nullptr,
                app->module,
                impl);

            if (!impl->handle) {
                auto res = ::GetLastError();
                win::DebugRes(res);
                NOVA_THROW("Error creating window: {:#x}", u32(HRESULT_FROM_WIN32(res)));
            }

            ::ShowWindow(impl->handle, SW_SHOW);
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

    Application Window::GetApplication() const
    {
        return impl->app;
    }

    void* Window::GetNativeHandle() const
    {
        return impl->handle;
    }

    Vec2U Window::GetSize(WindowPart part) const
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

    void Window::SetSize(Vec2U new_size, WindowPart part) const
    {
        if (part == WindowPart::Client) {
            RECT client;
            ::GetClientRect(impl->handle, &client);

            RECT window;
            ::GetWindowRect(impl->handle, &window);

            new_size.x += (window.right - window.left) - client.right;
            new_size.y += (window.bottom - window.top) - client.bottom;
        }

        ::SetWindowPos(impl->handle, HWND_NOTOPMOST, 0, 0,
            i32(new_size.x), i32(new_size.y),
            SWP_NOMOVE | SWP_NOOWNERZORDER);
    }

    Vec2I Window::GetPosition(WindowPart part) const
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

    void Window::SetPosition(Vec2I pos, WindowPart part) const
    {
        if (part == WindowPart::Client) {
            RECT window_rect;
            ::GetWindowRect(impl->handle, &window_rect);

            POINT client_point;
            ::ClientToScreen(impl->handle, &client_point);

            pos.x += window_rect.left - client_point.x;
            pos.y += window_rect.top - client_point.y;
        }

        ::SetWindowPos(impl->handle, HWND_NOTOPMOST, pos.x, pos.y, 0, 0,
            SWP_NOSIZE | SWP_NOOWNERZORDER);
    }

    void Window::SetCursor(Cursor cursor) const
    {
        // TODO: Track previous cursor, and update when entering/exiting windows
        // TODO: Handle transparency?

        LPWSTR resource;
        switch (cursor) {
            // TODO: Store and restore previous cursor
            break;case Cursor::Reset:      resource = IDC_ARROW;

            // TODO: Temporary, handle hiding/restoring cursor
            break;case Cursor::None: resource = IDC_APPSTARTING;

            break;case Cursor::Arrow:      resource = IDC_ARROW;
            break;case Cursor::IBeam:      resource = IDC_IBEAM;
            break;case Cursor::Hand:       resource = IDC_HAND;
            break;case Cursor::ResizeNS:   resource = IDC_SIZENS;
            break;case Cursor::ResizeEW:   resource = IDC_SIZEWE;
            break;case Cursor::ResizeNWSE: resource = IDC_SIZENWSE;
            break;case Cursor::ResizeNESW: resource = IDC_SIZENESW;
            break;case Cursor::ResizeAll:  resource = IDC_SIZEALL;
            break;case Cursor::NotAllowed: resource = IDC_NO;
        }

        // TODO: Preload cursors and keep handles around
        auto handle = ::LoadCursorW(nullptr, resource);

        ::SetCursor(handle);
    }

    void Window::SetTitle(std::string_view title) const
    {
        NOVA_STACK_POINT();
        ::SetWindowTextW(impl->handle, NOVA_STACK_TO_UTF16(title).data());
    }

    void Win32_UpdateStyles(HWND hwnd, u32 add, u32 remove, u32 add_ext, u32 remove_ext)
    {
        auto old = (u32)::GetWindowLongPtrW(hwnd, GWL_STYLE);
        ::SetWindowLongPtrW(hwnd, GWL_STYLE, LONG_PTR((old & ~remove) | add));

        auto old_ext = (u32)::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        ::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, LONG_PTR((old_ext & ~remove_ext) | add_ext));
    }

    void Window::SetDecorated(bool state) const
    {
        if (state) {
            Win32_UpdateStyles(impl->handle, WS_OVERLAPPEDWINDOW, 0, 0, 0);
        } else {
            Win32_UpdateStyles(impl->handle, 0, WS_OVERLAPPEDWINDOW, 0, 0);
        }
    }

    void Window::SetTransparent(bool state, Vec3U chroma_key) const
    {
        if (state) {
            Win32_UpdateStyles(impl->handle, 0, 0, WS_EX_LAYERED | WS_EX_TRANSPARENT, 0);
            ::SetLayeredWindowAttributes(impl->handle, RGB(chroma_key.r, chroma_key.g, chroma_key.b), 0, LWA_COLORKEY);
        } else {
            Win32_UpdateStyles(impl->handle, 0, 0, 0, WS_EX_LAYERED | WS_EX_TRANSPARENT);
        }
    }

    void Window::SetFullscreen(bool enabled) const
    {
        if (enabled) {
            {
                RECT rect;
                ::GetWindowRect(impl->handle, &rect);
                impl->restore.rect = {
                    .offset = { rect.left, rect.top },
                    .extent = { rect.right - rect.left, rect.bottom - rect.top }
                };
            }

            // Pick monitor based on window location
            auto monitor = impl->app.GetPrimaryDisplay();

            SetDecorated(false);
            SetPosition(monitor.GetPosition(), WindowPart::Window);
            SetSize(monitor.GetSize(), WindowPart::Client);
            ::ShowWindow(impl->handle, SW_MAXIMIZE);

        } else {
            SetDecorated(true);
            ::ShowWindow(impl->handle, SW_NORMAL);
            SetPosition(impl->restore.rect.offset, WindowPart::Window);
            SetSize(impl->restore.rect.extent, WindowPart::Window);
        }
    }
}