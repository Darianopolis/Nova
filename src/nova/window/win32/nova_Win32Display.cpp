#include "nova_Win32Window.hpp"

namespace nova
{
    Display Application::GetPrimaryDisplay() const
    {
        auto display = new Display::Impl;
        display->app = *this;

        display->monitor = MonitorFromPoint(POINT(0, 0), MONITOR_DEFAULTTOPRIMARY);

        return{ display };
    }

    Vec2I Display::GetSize() const
    {
        MONITORINFO info{};
        info.cbSize = sizeof(info);

        GetMonitorInfoW(impl->monitor, &info);

        return {
            info.rcMonitor.right - info.rcMonitor.left,
            info.rcMonitor.bottom - info.rcMonitor.top
        };
    }

    Vec2I Display::GetPosition() const
    {
        MONITORINFO info{};
        info.cbSize = sizeof(info);

        GetMonitorInfoW(impl->monitor, &info);

        return {
            info.rcMonitor.left,
            info.rcMonitor.top
        };
    }
}