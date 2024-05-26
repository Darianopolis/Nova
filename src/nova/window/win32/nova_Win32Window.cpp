#include "nova_Win32Window.hpp"

#include <nova/core/win32/nova_Win32.hpp>

#include <windowsx.h>
#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")

namespace
{
    nova::StringView Win32_WindowMessageToString(UINT msg)
    {
        switch (msg)
        {
            break;case WM_NULL: return "WM_NULL";
            break;case WM_CREATE: return "WM_CREATE";
            break;case WM_DESTROY: return "WM_DESTROY";
            break;case WM_MOVE: return "WM_MOVE";
            break;case WM_SIZE: return "WM_SIZE";
            break;case WM_ACTIVATE: return "WM_ACTIVATE";
            break;case WM_SETFOCUS: return "WM_SETFOCUS";
            break;case WM_KILLFOCUS: return "WM_KILLFOCUS";
            break;case WM_ENABLE: return "WM_ENABLE";
            break;case WM_SETREDRAW: return "WM_SETREDRAW";
            break;case WM_SETTEXT: return "WM_SETTEXT";
            break;case WM_GETTEXT: return "WM_GETTEXT";
            break;case WM_GETTEXTLENGTH: return "WM_GETTEXTLENGTH";
            break;case WM_PAINT: return "WM_PAINT";
            break;case WM_CLOSE: return "WM_CLOSE";
            break;case WM_QUERYENDSESSION: return "WM_QUERYENDSESSION";
            break;case WM_QUERYOPEN: return "WM_QUERYOPEN";
            break;case WM_ENDSESSION: return "WM_ENDSESSION";
            break;case WM_QUIT: return "WM_QUIT";
            break;case WM_ERASEBKGND: return "WM_ERASEBKGND";
            break;case WM_SYSCOLORCHANGE: return "WM_SYSCOLORCHANGE";
            break;case WM_SHOWWINDOW: return "WM_SHOWWINDOW";
            break;case WM_WININICHANGE: return "WM_WININICHANGE";
            // break;case WM_SETTINGCHANGE: return "WM_SETTINGCHANGE";
            break;case WM_DEVMODECHANGE: return "WM_DEVMODECHANGE";
            break;case WM_ACTIVATEAPP: return "WM_ACTIVATEAPP";
            break;case WM_FONTCHANGE: return "WM_FONTCHANGE";
            break;case WM_TIMECHANGE: return "WM_TIMECHANGE";
            break;case WM_CANCELMODE: return "WM_CANCELMODE";
            // break;case WM_SETCURSOR: return "WM_SETCURSOR";
            break;case WM_MOUSEACTIVATE: return "WM_MOUSEACTIVATE";
            break;case WM_CHILDACTIVATE: return "WM_CHILDACTIVATE";
            break;case WM_QUEUESYNC: return "WM_QUEUESYNC";
            break;case WM_GETMINMAXINFO: return "WM_GETMINMAXINFO";
            break;case WM_PAINTICON: return "WM_PAINTICON";
            break;case WM_ICONERASEBKGND: return "WM_ICONERASEBKGND";
            break;case WM_NEXTDLGCTL: return "WM_NEXTDLGCTL";
            break;case WM_SPOOLERSTATUS: return "WM_SPOOLERSTATUS";
            break;case WM_DRAWITEM: return "WM_DRAWITEM";
            break;case WM_MEASUREITEM: return "WM_MEASUREITEM";
            break;case WM_DELETEITEM: return "WM_DELETEITEM";
            break;case WM_VKEYTOITEM: return "WM_VKEYTOITEM";
            break;case WM_CHARTOITEM: return "WM_CHARTOITEM";
            break;case WM_SETFONT: return "WM_SETFONT";
            break;case WM_GETFONT: return "WM_GETFONT";
            break;case WM_SETHOTKEY: return "WM_SETHOTKEY";
            break;case WM_GETHOTKEY: return "WM_GETHOTKEY";
            break;case WM_QUERYDRAGICON: return "WM_QUERYDRAGICON";
            break;case WM_COMPAREITEM: return "WM_COMPAREITEM";
            break;case WM_GETOBJECT: return "WM_GETOBJECT";
            break;case WM_COMPACTING: return "WM_COMPACTING";
            break;case WM_COMMNOTIFY: return "WM_COMMNOTIFY";
            break;case WM_WINDOWPOSCHANGING: return "WM_WINDOWPOSCHANGING";
            break;case WM_WINDOWPOSCHANGED: return "WM_WINDOWPOSCHANGED";
            break;case WM_POWER: return "WM_POWER";
            break;case WM_COPYDATA: return "WM_COPYDATA";
            break;case WM_CANCELJOURNAL: return "WM_CANCELJOURNAL";
            break;case WM_NOTIFY: return "WM_NOTIFY";
            break;case WM_INPUTLANGCHANGEREQUEST: return "WM_INPUTLANGCHANGEREQUEST";
            break;case WM_INPUTLANGCHANGE: return "WM_INPUTLANGCHANGE";
            break;case WM_TCARD: return "WM_TCARD";
            break;case WM_HELP: return "WM_HELP";
            break;case WM_USERCHANGED: return "WM_USERCHANGED";
            break;case WM_NOTIFYFORMAT: return "WM_NOTIFYFORMAT";
            break;case WM_CONTEXTMENU: return "WM_CONTEXTMENU";
            break;case WM_STYLECHANGING: return "WM_STYLECHANGING";
            break;case WM_STYLECHANGED: return "WM_STYLECHANGED";
            break;case WM_DISPLAYCHANGE: return "WM_DISPLAYCHANGE";
            break;case WM_GETICON: return "WM_GETICON";
            break;case WM_SETICON: return "WM_SETICON";
            break;case WM_NCCREATE: return "WM_NCCREATE";
            break;case WM_NCDESTROY: return "WM_NCDESTROY";
            break;case WM_NCCALCSIZE: return "WM_NCCALCSIZE";
            // break;case WM_NCHITTEST: return "WM_NCHITTEST";
            break;case WM_NCPAINT: return "WM_NCPAINT";
            break;case WM_NCACTIVATE: return "WM_NCACTIVATE";
            break;case WM_GETDLGCODE: return "WM_GETDLGCODE";
            break;case WM_SYNCPAINT: return "WM_SYNCPAINT";
            // break;case WM_NCMOUSEMOVE: return "WM_NCMOUSEMOVE";
            break;case WM_NCLBUTTONDOWN: return "WM_NCLBUTTONDOWN";
            break;case WM_NCLBUTTONUP: return "WM_NCLBUTTONUP";
            break;case WM_NCLBUTTONDBLCLK: return "WM_NCLBUTTONDBLCLK";
            break;case WM_NCRBUTTONDOWN: return "WM_NCRBUTTONDOWN";
            break;case WM_NCRBUTTONUP: return "WM_NCRBUTTONUP";
            break;case WM_NCRBUTTONDBLCLK: return "WM_NCRBUTTONDBLCLK";
            break;case WM_NCMBUTTONDOWN: return "WM_NCMBUTTONDOWN";
            break;case WM_NCMBUTTONUP: return "WM_NCMBUTTONUP";
            break;case WM_NCMBUTTONDBLCLK: return "WM_NCMBUTTONDBLCLK";
            break;case WM_NCXBUTTONDOWN: return "WM_NCXBUTTONDOWN";
            break;case WM_NCXBUTTONUP: return "WM_NCXBUTTONUP";
            break;case WM_NCXBUTTONDBLCLK: return "WM_NCXBUTTONDBLCLK";
            break;case WM_INPUT_DEVICE_CHANGE: return "WM_INPUT_DEVICE_CHANGE";
            break;case WM_INPUT: return "WM_INPUT";
            break;case WM_KEYDOWN: return "WM_KEYDOWN";
            break;case WM_KEYUP: return "WM_KEYUP";
            break;case WM_CHAR: return "WM_CHAR";
            break;case WM_DEADCHAR: return "WM_DEADCHAR";
            break;case WM_SYSKEYDOWN: return "WM_SYSKEYDOWN";
            break;case WM_SYSKEYUP: return "WM_SYSKEYUP";
            break;case WM_SYSCHAR: return "WM_SYSCHAR";
            break;case WM_SYSDEADCHAR: return "WM_SYSDEADCHAR";
            break;case WM_UNICHAR: return "WM_UNICHAR";
            break;case WM_IME_STARTCOMPOSITION: return "WM_IME_STARTCOMPOSITION";
            break;case WM_IME_ENDCOMPOSITION: return "WM_IME_ENDCOMPOSITION";
            break;case WM_IME_COMPOSITION: return "WM_IME_COMPOSITION";
            break;case WM_INITDIALOG: return "WM_INITDIALOG";
            break;case WM_COMMAND: return "WM_COMMAND";
            break;case WM_SYSCOMMAND: return "WM_SYSCOMMAND";
            break;case WM_TIMER: return "WM_TIMER";
            break;case WM_HSCROLL: return "WM_HSCROLL";
            break;case WM_VSCROLL: return "WM_VSCROLL";
            break;case WM_INITMENU: return "WM_INITMENU";
            break;case WM_INITMENUPOPUP: return "WM_INITMENUPOPUP";
            break;case WM_GESTURE: return "WM_GESTURE";
            break;case WM_GESTURENOTIFY: return "WM_GESTURENOTIFY";
            break;case WM_MENUSELECT: return "WM_MENUSELECT";
            break;case WM_MENUCHAR: return "WM_MENUCHAR";
            break;case WM_ENTERIDLE: return "WM_ENTERIDLE";
            break;case WM_MENURBUTTONUP: return "WM_MENURBUTTONUP";
            break;case WM_MENUDRAG: return "WM_MENUDRAG";
            break;case WM_MENUGETOBJECT: return "WM_MENUGETOBJECT";
            break;case WM_UNINITMENUPOPUP: return "WM_UNINITMENUPOPUP";
            break;case WM_MENUCOMMAND: return "WM_MENUCOMMAND";
            break;case WM_CHANGEUISTATE: return "WM_CHANGEUISTATE";
            break;case WM_UPDATEUISTATE: return "WM_UPDATEUISTATE";
            break;case WM_QUERYUISTATE: return "WM_QUERYUISTATE";
            break;case WM_CTLCOLORMSGBOX: return "WM_CTLCOLORMSGBOX";
            break;case WM_CTLCOLOREDIT: return "WM_CTLCOLOREDIT";
            break;case WM_CTLCOLORLISTBOX: return "WM_CTLCOLORLISTBOX";
            break;case WM_CTLCOLORBTN: return "WM_CTLCOLORBTN";
            break;case WM_CTLCOLORDLG: return "WM_CTLCOLORDLG";
            break;case WM_CTLCOLORSCROLLBAR: return "WM_CTLCOLORSCROLLBAR";
            break;case WM_CTLCOLORSTATIC: return "WM_CTLCOLORSTATIC";
            // break;case WM_MOUSEMOVE: return "WM_MOUSEMOVE";
            break;case WM_LBUTTONDOWN: return "WM_LBUTTONDOWN";
            break;case WM_LBUTTONUP: return "WM_LBUTTONUP";
            break;case WM_LBUTTONDBLCLK: return "WM_LBUTTONDBLCLK";
            break;case WM_RBUTTONDOWN: return "WM_RBUTTONDOWN";
            break;case WM_RBUTTONUP: return "WM_RBUTTONUP";
            break;case WM_RBUTTONDBLCLK: return "WM_RBUTTONDBLCLK";
            break;case WM_MBUTTONDOWN: return "WM_MBUTTONDOWN";
            break;case WM_MBUTTONUP: return "WM_MBUTTONUP";
            break;case WM_MBUTTONDBLCLK: return "WM_MBUTTONDBLCLK";
            break;case WM_MOUSEWHEEL: return "WM_MOUSEWHEEL";
            break;case WM_XBUTTONDOWN: return "WM_XBUTTONDOWN";
            break;case WM_XBUTTONUP: return "WM_XBUTTONUP";
            break;case WM_XBUTTONDBLCLK: return "WM_XBUTTONDBLCLK";
            break;case WM_MOUSEHWHEEL: return "WM_MOUSEHWHEEL";
            break;case WM_PARENTNOTIFY: return "WM_PARENTNOTIFY";
            break;case WM_ENTERMENULOOP: return "WM_ENTERMENULOOP";
            break;case WM_EXITMENULOOP: return "WM_EXITMENULOOP";
            break;case WM_NEXTMENU: return "WM_NEXTMENU";
            break;case WM_SIZING: return "WM_SIZING";
            break;case WM_CAPTURECHANGED: return "WM_CAPTURECHANGED";
            break;case WM_MOVING: return "WM_MOVING";
            break;case WM_POWERBROADCAST: return "WM_POWERBROADCAST";
            break;case WM_DEVICECHANGE: return "WM_DEVICECHANGE";
            break;case WM_MDICREATE: return "WM_MDICREATE";
            break;case WM_MDIDESTROY: return "WM_MDIDESTROY";
            break;case WM_MDIACTIVATE: return "WM_MDIACTIVATE";
            break;case WM_MDIRESTORE: return "WM_MDIRESTORE";
            break;case WM_MDINEXT: return "WM_MDINEXT";
            break;case WM_MDIMAXIMIZE: return "WM_MDIMAXIMIZE";
            break;case WM_MDITILE: return "WM_MDITILE";
            break;case WM_MDICASCADE: return "WM_MDICASCADE";
            break;case WM_MDIICONARRANGE: return "WM_MDIICONARRANGE";
            break;case WM_MDIGETACTIVE: return "WM_MDIGETACTIVE";
            break;case WM_MDISETMENU: return "WM_MDISETMENU";
            break;case WM_ENTERSIZEMOVE: return "WM_ENTERSIZEMOVE";
            break;case WM_EXITSIZEMOVE: return "WM_EXITSIZEMOVE";
            break;case WM_DROPFILES: return "WM_DROPFILES";
            break;case WM_MDIREFRESHMENU: return "WM_MDIREFRESHMENU";
            break;case WM_POINTERDEVICECHANGE: return "WM_POINTERDEVICECHANGE";
            break;case WM_POINTERDEVICEINRANGE: return "WM_POINTERDEVICEINRANGE";
            break;case WM_POINTERDEVICEOUTOFRANGE: return "WM_POINTERDEVICEOUTOFRANGE";
            break;case WM_TOUCH: return "WM_TOUCH";
            break;case WM_NCPOINTERUPDATE: return "WM_NCPOINTERUPDATE";
            break;case WM_NCPOINTERDOWN: return "WM_NCPOINTERDOWN";
            break;case WM_NCPOINTERUP: return "WM_NCPOINTERUP";
            break;case WM_POINTERUPDATE: return "WM_POINTERUPDATE";
            break;case WM_POINTERDOWN: return "WM_POINTERDOWN";
            break;case WM_POINTERUP: return "WM_POINTERUP";
            break;case WM_POINTERENTER: return "WM_POINTERENTER";
            break;case WM_POINTERLEAVE: return "WM_POINTERLEAVE";
            break;case WM_POINTERACTIVATE: return "WM_POINTERACTIVATE";
            break;case WM_POINTERCAPTURECHANGED: return "WM_POINTERCAPTURECHANGED";
            break;case WM_TOUCHHITTESTING: return "WM_TOUCHHITTESTING";
            break;case WM_POINTERWHEEL: return "WM_POINTERWHEEL";
            break;case WM_POINTERHWHEEL: return "WM_POINTERHWHEEL";
            break;case WM_POINTERROUTEDTO: return "WM_POINTERROUTEDTO";
            break;case WM_POINTERROUTEDAWAY: return "WM_POINTERROUTEDAWAY";
            break;case WM_POINTERROUTEDRELEASED: return "WM_POINTERROUTEDRELEASED";
            break;case WM_IME_SETCONTEXT: return "WM_IME_SETCONTEXT";
            break;case WM_IME_NOTIFY: return "WM_IME_NOTIFY";
            break;case WM_IME_CONTROL: return "WM_IME_CONTROL";
            break;case WM_IME_COMPOSITIONFULL: return "WM_IME_COMPOSITIONFULL";
            break;case WM_IME_SELECT: return "WM_IME_SELECT";
            break;case WM_IME_CHAR: return "WM_IME_CHAR";
            break;case WM_IME_REQUEST: return "WM_IME_REQUEST";
            break;case WM_IME_KEYDOWN: return "WM_IME_KEYDOWN";
            break;case WM_IME_KEYUP: return "WM_IME_KEYUP";
            break;case WM_MOUSEHOVER: return "WM_MOUSEHOVER";
            break;case WM_MOUSELEAVE: return "WM_MOUSELEAVE";
            break;case WM_NCMOUSEHOVER: return "WM_NCMOUSEHOVER";
            break;case WM_NCMOUSELEAVE: return "WM_NCMOUSELEAVE";
            break;case WM_WTSSESSION_CHANGE: return "WM_WTSSESSION_CHANGE";
            break;case WM_TABLET_FIRST: return "WM_TABLET_FIRST";
            break;case WM_TABLET_LAST: return "WM_TABLET_LAST";
            break;case WM_DPICHANGED: return "WM_DPICHANGED";
            break;case WM_DPICHANGED_BEFOREPARENT: return "WM_DPICHANGED_BEFOREPARENT";
            break;case WM_DPICHANGED_AFTERPARENT: return "WM_DPICHANGED_AFTERPARENT";
            break;case WM_GETDPISCALEDSIZE: return "WM_GETDPISCALEDSIZE";
            break;case WM_CUT: return "WM_CUT";
            break;case WM_COPY: return "WM_COPY";
            break;case WM_PASTE: return "WM_PASTE";
            break;case WM_CLEAR: return "WM_CLEAR";
            break;case WM_UNDO: return "WM_UNDO";
            break;case WM_RENDERFORMAT: return "WM_RENDERFORMAT";
            break;case WM_RENDERALLFORMATS: return "WM_RENDERALLFORMATS";
            break;case WM_DESTROYCLIPBOARD: return "WM_DESTROYCLIPBOARD";
            break;case WM_DRAWCLIPBOARD: return "WM_DRAWCLIPBOARD";
            break;case WM_PAINTCLIPBOARD: return "WM_PAINTCLIPBOARD";
            break;case WM_VSCROLLCLIPBOARD: return "WM_VSCROLLCLIPBOARD";
            break;case WM_SIZECLIPBOARD: return "WM_SIZECLIPBOARD";
            break;case WM_ASKCBFORMATNAME: return "WM_ASKCBFORMATNAME";
            break;case WM_CHANGECBCHAIN: return "WM_CHANGECBCHAIN";
            break;case WM_HSCROLLCLIPBOARD: return "WM_HSCROLLCLIPBOARD";
            break;case WM_QUERYNEWPALETTE: return "WM_QUERYNEWPALETTE";
            break;case WM_PALETTEISCHANGING: return "WM_PALETTEISCHANGING";
            break;case WM_PALETTECHANGED: return "WM_PALETTECHANGED";
            break;case WM_HOTKEY: return "WM_HOTKEY";
            break;case WM_PRINT: return "WM_PRINT";
            break;case WM_PRINTCLIENT: return "WM_PRINTCLIENT";
            break;case WM_APPCOMMAND: return "WM_APPCOMMAND";
            break;case WM_THEMECHANGED: return "WM_THEMECHANGED";
            break;case WM_CLIPBOARDUPDATE: return "WM_CLIPBOARDUPDATE";
            break;case WM_DWMCOMPOSITIONCHANGED: return "WM_DWMCOMPOSITIONCHANGED";
            break;case WM_DWMNCRENDERINGCHANGED: return "WM_DWMNCRENDERINGCHANGED";
            break;case WM_DWMCOLORIZATIONCOLORCHANGED: return "WM_DWMCOLORIZATIONCOLORCHANGED";
            break;case WM_DWMWINDOWMAXIMIZEDCHANGE: return "WM_DWMWINDOWMAXIMIZEDCHANGE";
            break;case WM_DWMSENDICONICTHUMBNAIL: return "WM_DWMSENDICONICTHUMBNAIL";
            break;case WM_DWMSENDICONICLIVEPREVIEWBITMAP: return "WM_DWMSENDICONICLIVEPREVIEWBITMAP";
            break;case WM_GETTITLEBARINFOEX: return "WM_GETTITLEBARINFOEX";
            break;case WM_HANDHELDFIRST: return "WM_HANDHELDFIRST";
            break;case WM_HANDHELDLAST: return "WM_HANDHELDLAST";
            break;case WM_AFXFIRST: return "WM_AFXFIRST";
            break;case WM_AFXLAST: return "WM_AFXLAST";
            break;case WM_PENWINFIRST: return "WM_PENWINFIRST";
            break;case WM_PENWINLAST: return "WM_PENWINLAST";
        }
        return "";
    }
}

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
        //     if (str.Size()) {
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
        }

        return ::DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    Window Window::Create(nova::Application app)
    {
        auto impl = new Impl;
        NOVA_DEFER(&) { if (exceptions) delete impl; };

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

        // TODO
        ::DestroyWindow(impl->handle);

        delete impl;
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
        ::ShowWindow(impl->handle, state ? SW_SHOW : SW_HIDE);

        return *this;
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
        if (part == WindowPart::Client) {
            RECT window_rect;
            ::GetWindowRect(impl->handle, &window_rect);

            POINT client_point = {};
            ::ClientToScreen(impl->handle, &client_point);

            pos.x += window_rect.left - client_point.x;
            pos.y += window_rect.top - client_point.y;
        }

        ::SetWindowPos(impl->handle, HWND_NOTOPMOST, pos.x, pos.y, 0, 0,
            SWP_NOSIZE | SWP_NOOWNERZORDER);

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

    Window Window::SetTransparency(TransparencyMode mode, Vec3U chroma_key) const
    {
        switch (mode) {
            break;case TransparencyMode::Disabled:
                Win32_UpdateStyles(impl->handle, 0, 0, 0, WS_EX_LAYERED | WS_EX_TRANSPARENT);
            break;case TransparencyMode::ChromaKey:
                Win32_UpdateStyles(impl->handle, 0, 0, WS_EX_LAYERED | WS_EX_TRANSPARENT, 0);
                ::SetLayeredWindowAttributes(impl->handle, RGB(chroma_key.r, chroma_key.g, chroma_key.b), 0, LWA_COLORKEY);
            break;case TransparencyMode::PerPixel:
                // Must clear layered bit in case last state was ChromaKey
                Win32_UpdateStyles(impl->handle, 0, 0, 0, WS_EX_LAYERED | WS_EX_TRANSPARENT);
                Win32_UpdateStyles(impl->handle, 0, 0, WS_EX_LAYERED, 0);
        }

        return *this;
    }

    Window Window::SetFullscreen(bool enabled) const
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

            // TODO: Pick monitor based on window location
            auto monitor = impl->app.PrimaryDisplay();

            SetDecorate(false);
            SetPosition(monitor.Position(), WindowPart::Window);
            SetSize(monitor.Size(), WindowPart::Client);
            ::ShowWindow(impl->handle, SW_MAXIMIZE);

        } else {
            SetDecorate(true);
            ::ShowWindow(impl->handle, SW_NORMAL);
            SetPosition(impl->restore.rect.offset, WindowPart::Window);
            SetSize(impl->restore.rect.extent, WindowPart::Window);
        }

        return *this;
    }
}