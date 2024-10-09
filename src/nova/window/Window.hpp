#pragma once

#include <nova/core/Core.hpp>

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

    enum class WindowPart
    {
        Window,
        Client,
    };

    enum class TransparencyMode
    {
        Disabled,
        ChromaKey,
        Static,
        PerPixel,
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

        VolumeMute,
        VolumeUp,
        VolumeDown,
        MediaStop,
        MediaPlayPause,
        MediaPrev,
        MediaNext,

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
        WindowClosing,

        Hotkey, // TODO: Registering and payload!

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

    struct HotkeyEvent
    {
        i32 id;
    };

    struct AppEvent
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
            HotkeyEvent        hotkey;
        };
        bool consumed = false;
    };

// -----------------------------------------------------------------------------

    using AppCallback = std::function<void(const AppEvent&)>;

    struct Application : Handle<Application>
    {
        static Application Create();
        void Destroy();

        // TODO: Unregister callback
        // TODO: Sort
        // TODO: Consuming events
        void AddCallback(AppCallback callback) const;

        void WaitForEvents() const;
        bool ProcessEvents() const;

        VirtualKey ToVirtualKey(InputChannel channel) const;
        StringView VirtualKeyToString(VirtualKey key) const;
        bool IsVirtualKeyDown(VirtualKey button) const;

        InputChannelState InputState(InputChannel) const;

        void DebugInputState() const;

        // TODO: Provide enumeration of displays; Get display for point/rect/window
        Display PrimaryDisplay() const;
    };

    struct Display : Handle<Display>
    {
        Vec2I Size() const;
        Vec2I Position() const;

        // TODO: Additional display info
    };

    /*

    TODO: Refactor state

    Window Modes

    -----------------------------------------

    - decorations
        - disabled
        - light / dark
        - system themed

    - synchronize move + size + present

    - transparency mode
        - opaque
        - transparent
        - blur

    - clickthrough
        - never
        - always
        - alpha-threshold
        - callback

    -----------------------------------------

    - allow tearing
    - wait for vsync

    - alpha mode
        - opaque
        - pre-multiplied
        - post-mutliplied

    -----------------------------------------

    - taskbar icon

    - state
        - hidden
        - iconified (can you be in this state with a hidden taskbar icon?)
        - windowed
        - fullscreen

    */

    struct Window : Handle<Window>
    {
        static Window Create(Application);
        void Destroy();

        Application Application() const;
        void* NativeHandle() const;

        bool Minimized() const;
        Window Show(bool state) const;

        void Focus() const;

        Vec2U Size(WindowPart) const;
        Window SetSize(Vec2U size, WindowPart) const;

        Vec2I Position(WindowPart) const;
        Window SetPosition(Vec2I pos, WindowPart) const;

        StringView Title() const;
        Window SetTitle(std::string title) const;

        Window SetDarkMode(bool state) const;

        Window SetCursor(Cursor cursor) const;
        Window SetDecorate(bool state) const;
        Window SetTransparency(TransparencyMode mode, Vec3U chroma_key = {}, f32 alpha = 1.f) const;
        Window SetFullscreen(bool enabled) const;
        Window SetBackgroundColor(bool enabled, Vec3 color = {}) const;
        Window SetTopmost(bool topmost) const;

        // Control whether a window shows up in the taskbar and/or task switcher
        Window SetAppWindow(bool appwindow) const;
    };
}
