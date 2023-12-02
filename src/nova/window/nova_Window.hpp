#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Containers.hpp>
#include <nova/core/nova_Math.hpp>

namespace nova
{
    using HApplication = Handle<struct Application>;
    using HWindow = Handle<struct Window>;
    using HDisplay = Handle<struct Display>;

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

    enum class Cursor
    {
        Reset = 0,

        None,

        Arrow,
        IBeam,
        Hand,
        ResizeNS,
        ResizeEW,
        ResizeNWSE,
        ResizeNESW,
        ResizeAll,
        NotAllowed,
    };

    struct InputChannel
    {
        u32 code;
        u32 category;
        u64 device;
    };

    enum class VirtualKey : u32
    {
        Unknown,

        MousePrimary,
        MouseMiddle,
        MouseSecondary,

        MouseForward,
        MouseBack,

        Tab,

        Left,
        Right,
        Up,
        Down,

        PageUp,
        PageDown,

        Home,
        End,
        Insert,
        Delete,
        Backspace,
        Space,
        Enter,
        Escape,

        Apostrophe,
        Comma,
        Minus,
        Period,
        Slash,
        Semicolon,
        Backtick,
        Equal,
        LeftBracket,
        Backslash,
        RightBracket,
        Hash,

        CapsLock,
        ScrollLock,
        NumLock,
        PrintScreen,
        Pause,

        Num0,
        Num1,
        Num2,
        Num3,
        Num4,
        Num5,
        Num6,
        Num7,
        Num8,
        Num9,

        NumDecimal,
        NumDivide,
        NumMultiply,
        NumSubtract,
        NumAdd,
        NumEnter,
        NumEqual,

        Shift,
        Control,
        Alt,
        Super,

        LeftShift,
        LeftControl,
        LeftAlt,
        LeftSuper,

        RightShift,
        RightControl,
        RightAlt,
        RightSuper,

        _0, _1, _2, _3, _4, _5, _6, _7, _8, _9,
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        F1,  F2,  F3,  F4,  F5,  F6,  F7,  F8,  F9,  F10, F11, F12,
        F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,
    };

    enum class Modifier
    {
        Shift,
        Ctrl,
        Alt,
        Super,
    };

    inline
    bool IsMouseButton(VirtualKey button)
    {
        return button == VirtualKey::MousePrimary
            || button == VirtualKey::MouseSecondary
            || button == VirtualKey::MouseMiddle;
    }

// -----------------------------------------------------------------------------

    enum class WindowPart
    {
        Window,
        Client,
    };

// -----------------------------------------------------------------------------

    enum class EventType
    {
        Text,
        Input,
        MouseMove,
        MouseScroll,

        WindowFocus,
        WindowResized,
        WindowState,

        WindowCloseRequested,

        Shutdown,
    };

    struct TextEvent
    {
        c8 text[5];
    };

    struct InputChannelState
    {
        InputChannel channel;
        bool         pressed;
        bool         toggled;
        float          value;
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
        HApplication app;
        HWindow   window;
        EventType   type;
        union
        {
            TextEvent            text;
            InputChannelState   input;
            MouseMoveEvent mouse_move;
            MouseScrollEvent   scroll;
            WindowFocusEvent    focus;
        };
        bool consumed = false;
    };

// -----------------------------------------------------------------------------

    using Callback = std::function<void(const Event&)>;

    struct Application : Handle<Application>
    {
        static Application Create();
        void Destroy();

        void SetCallback(Callback callback) const;

        void WaitEvents() const;
        void PollEvents() const;

        bool IsRunning() const;

        VirtualKey ToVirtualKey(InputChannel channel) const;
        std::string_view VirtualKeyToString(VirtualKey key) const;
        bool IsVirtualKeyDown(VirtualKey button) const;

        InputChannelState GetInputState(InputChannel) const;

        // TODO: Provide enumeration of displays; Get display for point/rect/window
        Display GetPrimaryDisplay() const;
    };

    struct Display : Handle<Display>
    {
        Vec2I GetSize() const;
        Vec2I GetPosition() const;

        // TODO: Additional display info
    };

    struct Window : Handle<Window>
    {
        static Window Create(Application, const WindowInfo&);
        void Destroy();

        Application GetApplication() const;
        void* GetNativeHandle() const;

        Vec2U GetSize(WindowPart) const;
        void SetSize(Vec2U size, WindowPart) const;

        Vec2I GetPosition(WindowPart) const;
        void SetPosition(Vec2I pos, WindowPart) const;

        void SetCursor(Cursor cursor) const;
        void SetTitle(std::string_view title) const;
        void SetDecorated(bool state) const;
        // TODO: Chroma key is bad and you should feel bad
        void SetTransparent(bool state, Vec3U chroma_key) const;
        void SetFullscreen(bool enabled) const;
    };
}