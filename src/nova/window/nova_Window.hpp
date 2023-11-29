#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Containers.hpp>
#include <nova/core/nova_Math.hpp>

namespace nova
{
    using HApplication = Handle<struct Application>;
    using HWindow = Handle<struct Window>;

// -----------------------------------------------------------------------------

    enum class WindowState
    {
        Closed,
        Minimized,
        Normal,
        Fullscreen,
    };

    struct WindowInfo
    {
        std::string_view title;
        Vec2U             size = {};
    };

// -----------------------------------------------------------------------------

    enum class EventType
    {
        Text,
        Button,
        MouseMove,
        MouseScroll,

        WindowFocus,
        WindowResized,
        WindowState,

        WindowClosedRequested,
    };

    struct TextEvent
    {
        c32 codepoint;
    };

    struct Button
    {
        u64 code;
    };

    struct ButtonEvent
    {
        Button button;
        u32    repeat;
        bool  pressed;
    };

    struct MouseMoveEvent
    {
        Vec2  position;
        Vec2     delta;
        bool in_client;
        bool  dragging;
    };

    struct MouseScrollEvent
    {
        Vec2 scrolled;
    };

    struct WindowFocusEvent
    {
        HWindow  losing;
        HWindow gaining;
    };

    struct Event
    {
        EventType type;
        union
        {
            TextEvent            text;
            ButtonEvent        button;
            MouseMoveEvent mouse_move;
            MouseScrollEvent   scroll;
            WindowFocusEvent    focus;
        };
        bool consumed = false;
    };

// -----------------------------------------------------------------------------

    struct Application : Handle<Application>
    {
        static Application Create();
        void Destroy();

        void Run() const;
    };

    struct Window : Handle<Window>
    {
        static Window Create(Application, const WindowInfo&);
        void Destroy();

        Vec2U GetSize() const;
        void SetSize(Vec2U size) const;

        void SetFullscreen(bool enabled) const;
    };
}