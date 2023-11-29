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
            .lpszClassName = Win32WndClassName,
        };

        detail::CheckNot(ATOM(0), RegisterClassW(&class_info));

        return { impl };
    }

    void Application::Destroy()
    {
        if (!impl) return;
        delete impl;
    }

    void Application::Run() const
    {
        MSG msg = {};
        while (GetMessageW(&msg, nullptr, 0, 0) != 0)
        {
NOVA_DEBUG();
            switch (msg.message)
            {
                break;case WM_HOTKEY:
                    NOVA_LOG("hotkey");
                break;case WM_TIMER:
                    NOVA_LOG("timer");
                break;case WM_QUIT:
                    NOVA_LOG("quitting");
                    for (auto& window : impl->windows) {
                        window.Destroy();
                    }
                break;default:
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
            }
NOVA_DEBUG();
        }
    }
}