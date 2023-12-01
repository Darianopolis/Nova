#include "nova_Win32Window.hpp"

namespace nova
{
    Application Application::Create()
    {
        auto impl = new Impl;

        impl->module = GetModuleHandleW(nullptr);

        detail::Check(TRUE, SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));

        WNDCLASSW class_info {
            .lpfnWndProc = Window::Impl::WindowProc,
            .cbWndExtra = 8,
            .hInstance = impl->module,
            .hCursor = LoadCursorW(nullptr, IDC_ARROW),
            .lpszClassName = Win32WndClassName,
        };

        detail::CheckNot(ATOM(0), RegisterClassW(&class_info));

        SetConsoleOutputCP(CP_UTF8);

        return { impl };
    }

    void Application::Destroy()
    {
        if (!impl) return;
        delete impl;
    }

    void Application::WaitEvents() const
    {
        WaitMessage();
    }

    void Application::SetCallback(Callback callback) const
    {
        impl->callback = std::move(callback);
    }

    void Application::Impl::Send(Event event)
    {
        event.app = Application(this);

        switch (event.type)
        {
            break;case EventType::Text:
                {
                    NOVA_LOG("Application :: Text({})", event.text.text);
                }

            break;case EventType::Button:
                {
                    const char* mapped = "Unknown";
                    switch (Application(this).GetButton(event.button.code)) {
                        break;case Button::MousePrimary:   mapped = "MousePrimary";
                        break;case Button::MouseSecondary: mapped = "MouseSecondary";
                        break;case Button::MouseMiddle:    mapped = "MouseMiddle";

                        break;case Button::Tab: mapped = "Tab";

                        break;case Button::Left:  mapped = "Left";
                        break;case Button::Right: mapped = "Right";
                        break;case Button::Up:    mapped = "Up";
                        break;case Button::Down:  mapped = "Down";

                        break;case Button::PageUp:    mapped = "PageUp";
                        break;case Button::PageDown:  mapped = "PageDown";
                        break;case Button::Home:      mapped = "Home";
                        break;case Button::End:       mapped = "End";
                        break;case Button::Insert:    mapped = "Insert";
                        break;case Button::Delete:    mapped = "Delete";
                        break;case Button::Backspace: mapped = "Backspace";
                        break;case Button::Space:     mapped = "Space";
                        break;case Button::Enter:     mapped = "Enter";
                        break;case Button::Escape:    mapped = "Escape";

                        break;case Button::LeftShift:   mapped = "LeftShift";
                        break;case Button::LeftControl: mapped = "LeftControl";
                        break;case Button::LeftAlt:     mapped = "LeftAlt";
                        break;case Button::LeftSuper:   mapped = "LeftSuper";

                        break;case Button::RightShift:   mapped = "RightShift";
                        break;case Button::RightControl: mapped = "RightControl";
                        break;case Button::RightAlt:     mapped = "RightAlt";
                        break;case Button::RightSuper:   mapped = "RightSuper";
                    }
                    NOVA_LOG("Application :: Button(code = {0} ({0:#x}), repeat = {1}, pressed = {2}, mapped = {3})",
                        event.button.code, event.button.repeat, event.button.pressed, mapped);
                }

            // break;case EventType::MouseMove:
            //     {
            //         NOVA_LOG("Application :: MouseMove({}, {})", event.mouse_move.position.x, event.mouse_move.position.y);
            //     }

            break;case EventType::MouseScroll:
                {
                    NOVA_LOG("Application :: MouseScroll({}, {})", event.scroll.scrolled.x, event.scroll.scrolled.y);
                }

            break;case EventType::WindowFocus:
                {
                    NOVA_LOG("Application :: WindowFocus(gained = {})", bool(event.focus.gaining));
                }

            break;case EventType::WindowResized:
                {
                    NOVA_LOG("Application :: WindowResized(...)");
                }

            break;case EventType::WindowState:
                {
                    NOVA_LOG("Application :: WindowState(...)");
                }

            break;case EventType::WindowClosedRequested:
                {
                    NOVA_LOG("Application :: WindowCloseRequested(...)");
                }

            break;case EventType::Shutdown:
                {
                    NOVA_LOG("Application :: Shutdown");
                }

        }

        if (callback) {
            callback(event);
        }
    }

    void Application::PollEvents() const
    {
        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            switch (msg.message) {
                break;case WM_HOTKEY:
                    NOVA_LOG("hotkey");
                break;case WM_TIMER:
                    // NOVA_LOG("timer");
                break;case WM_QUIT:
                    NOVA_LOG("quitting");
                    impl->running = false;
                    impl->Send({ .type = EventType::Shutdown });
                break;default:
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
            }
        }
    }

    bool Application::IsRunning() const
    {
        return impl->running;
    }
}