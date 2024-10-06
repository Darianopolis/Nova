#include "nova_Win32Window.hpp"

#include "nova_Win32WindowHelpers.hpp"

#include <nova/core/win32/nova_Win32.hpp>

namespace nova
{
    Application Application::Create()
    {
        auto impl = new Impl;

        impl->module = ::GetModuleHandleW(nullptr);

        impl->InitMappings();

        if (!::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            NOVA_THROW("Failed to set process DPI awareness context. Error: {}",
                win::HResultToString(HRESULT_FROM_WIN32(GetLastError())));
        }

        WNDCLASSW class_info {
            .lpfnWndProc = Window::Impl::WindowProc,
            .cbWndExtra = 8,
            .hInstance = impl->module,
            .hCursor = ::LoadCursorW(nullptr, IDC_ARROW),
            .lpszClassName = Win32WndClassName,
        };

        if (!::RegisterClassW(&class_info)) {
            NOVA_THROW("Failed to register class. Error: {}",
                win::HResultToString(HRESULT_FROM_WIN32(GetLastError())));
        }

        // impl->InitGameInput();

        return { impl };
    }

    void Application::Destroy()
    {
        if (!impl) return;

        // impl->DestroyGameInput();

        delete impl;
    }

    void Application::AddCallback(AppCallback callback) const
    {
        impl->callbacks.emplace_back(std::move(callback));
    }

    void Application::Impl::RemoveWindow(Window window)
    {
        std::erase(windows, window);
        if (windows.empty()) {
            ::PostQuitMessage(0);
        }
    }

    void Application::WaitForEvents() const
    {
        if (impl->running) {
            ::WaitMessage();
        }
    }

    void Application::Impl::Send(AppEvent event)
    {
        event.app = Application(this);

        if (trace_events) {
            switch (event.type)
            {
                break;case EventType::Text:
                    {
                        Log("Application :: Text({})", event.text.text);
                    }

                break;case EventType::Input:
                    {
                        auto app = Application(this);

                        Log("Application :: Button(code = {0} ({0:#x}), pressed = {1}, mapped = {2})",
                            event.input.channel.code, event.input.pressed,
                            app.VirtualKeyToString(app.ToVirtualKey(event.input.channel)));
                    }

                // break;case EventType::MouseMove:
                //     {
                //         Log("Application :: MouseMove({}, {})", event.mouse_move.position.x, event.mouse_move.position.y);
                //     }

                break;case EventType::MouseScroll:
                    {
                        Log("Application :: MouseScroll({}, {})", event.scroll.scrolled.x, event.scroll.scrolled.y);
                    }

                break;case EventType::WindowFocus:
                    {
                        Log("Application :: WindowFocus(gained = {})", bool(event.focus.gaining));
                    }

                break;case EventType::WindowResized:
                    {
                        Log("Application :: WindowResized(...)");
                    }

                break;case EventType::WindowState:
                    {
                        Log("Application :: WindowState(...)");
                    }

                break;case EventType::WindowCloseRequested:
                    {
                        Log("Application :: WindowCloseRequested(...)");
                    }

                break;case EventType::Shutdown:
                    {
                        Log("Application :: Shutdown");
                    }
            }
        }

        for (auto& callback : callbacks) {
            callback(event);
        }
    }

    bool Application::ProcessEvents() const
    {
        if (impl->running) {
            MSG msg = {};
            while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                // {
                //     auto str = Win32_WindowMessageToString(msg.message);
                //     if (str.size()) {
                //         Log("MSG: {0} ({0:#x} :: {1}), WPARAM: {2} ({2:#x}), LPARAM: {3} ({3:#x})", msg.message, str, msg.wParam, msg.lParam);
                //     }
                // }
                switch (msg.message) {
                    break;case WM_HOTKEY:
                        // TODO: Payload!
                        impl->Send({ .type = EventType::Hotkey, .hotkey = { i32(msg.wParam) } });
                    break;case WM_TIMER:
                        // Log("timer");
                    break;case WM_QUIT:
                        impl->running = false;
                        impl->Send({ .type = EventType::Shutdown });
                    break;default:
                        ::TranslateMessage(&msg);
                        ::DispatchMessageW(&msg);
                }
            }
        }

        return impl->running;
    }
}