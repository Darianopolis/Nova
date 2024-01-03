#include "nova_Win32Window.hpp"

namespace nova
{
    struct Win32VirtualKeyMapping {
        VirtualKey nova_mapping;
        u32       win32_mapping;
        std::string_view   name;
    };

    constexpr auto Win32VirtualKeyMappings = std::to_array<Win32VirtualKeyMapping>({
        { VirtualKey::MousePrimary,   VK_LBUTTON,  "MousePrimary"   },
        { VirtualKey::MouseMiddle,    VK_MBUTTON,  "MouseMiddle"    },
        { VirtualKey::MouseSecondary, VK_RBUTTON,  "MouseSecondary" },
        { VirtualKey::MouseForward,   VK_XBUTTON1, "MouseForward"   },
        { VirtualKey::MouseBack,      VK_XBUTTON2, "MouseBack"      },
// -------------------------------------------------------------------------------------------------------------
        { VirtualKey::Tab,       VK_TAB,    "Tab"       },
        { VirtualKey::Left,      VK_LEFT,   "Left"      },
        { VirtualKey::Right,     VK_RIGHT,  "Right"     },
        { VirtualKey::Up,        VK_UP,     "Up"        },
        { VirtualKey::Down,      VK_DOWN,   "Down"      },
        { VirtualKey::PageUp,    VK_PRIOR,  "PageUp"    },
        { VirtualKey::PageDown,  VK_NEXT,   "PageDown"  },
        { VirtualKey::Home,      VK_HOME,   "Home"      },
        { VirtualKey::End,       VK_END,    "End"       },
        { VirtualKey::Insert,    VK_INSERT, "Insert"    },
        { VirtualKey::Delete,    VK_DELETE, "Delete"    },
        { VirtualKey::Backspace, VK_BACK,   "Backspace" },
        { VirtualKey::Space,     VK_SPACE,  "Space"     },
        { VirtualKey::Enter,     VK_RETURN, "Enter"     },
        { VirtualKey::Escape,    VK_ESCAPE, "Escape"    },
// -------------------------------------------------------------------------------------------------------------
        { VirtualKey::Comma,  VK_OEM_COMMA,  "Comma"  },
        { VirtualKey::Minus,  VK_OEM_MINUS,  "Minus"  },
        { VirtualKey::Period, VK_OEM_PERIOD, "Period" },
        { VirtualKey::Equal,  VK_OEM_PLUS,   "Equal"  },
// -------------------------------------------------------------------------------------------------------------
        { VirtualKey::Semicolon,    VK_OEM_1, "Semicolon"    }, // * (OK for assuming qwerty)
        { VirtualKey::Slash,        VK_OEM_2, "Slash"        }, // *
        { VirtualKey::Apostrophe,   VK_OEM_3, "Apostrophe"   }, // *
        { VirtualKey::LeftBracket,  VK_OEM_4, "LeftBracket"  }, // *
        { VirtualKey::Backslash,    VK_OEM_5, "Backslash"    }, // *
        { VirtualKey::RightBracket, VK_OEM_6, "RightBracket" }, // *
        { VirtualKey::Hash,         VK_OEM_7, "Hash"         }, // *
        { VirtualKey::Backtick,     VK_OEM_8, "Backtick"     }, // * (Backtick on UK-ISO, Tilde on US keyboards)
// -------------------------------------------------------------------------------------------------------------
        { VirtualKey::CapsLock,    VK_CAPITAL, "CapsLock"    },
        { VirtualKey::ScrollLock,  VK_SCROLL,  "ScrollLock"  },
        { VirtualKey::NumLock,     VK_NUMLOCK, "NumLock"     },
        { VirtualKey::PrintScreen, VK_PRINT,   "PrintScreen" },
        { VirtualKey::Pause,       VK_PAUSE,   "Pause"       },
// -------------------------------------------------------------------------------------------------------------
        { VirtualKey::Num0,        VK_NUMPAD0,  "Num0"        },
        { VirtualKey::Num1,        VK_NUMPAD1,  "Num1"        },
        { VirtualKey::Num2,        VK_NUMPAD2,  "Num2"        },
        { VirtualKey::Num3,        VK_NUMPAD3,  "Num3"        },
        { VirtualKey::Num4,        VK_NUMPAD4,  "Num4"        },
        { VirtualKey::Num5,        VK_NUMPAD5,  "Num5"        },
        { VirtualKey::Num6,        VK_NUMPAD6,  "Num6"        },
        { VirtualKey::Num7,        VK_NUMPAD7,  "Num7"        },
        { VirtualKey::Num8,        VK_NUMPAD8,  "Num8"        },
        { VirtualKey::Num9,        VK_NUMPAD9,  "Num9"        },
        { VirtualKey::NumDecimal,  VK_DECIMAL,  "NumDecimal"  },
        { VirtualKey::NumDivide,   VK_DIVIDE,   "NumDivide"   },
        { VirtualKey::NumMultiply, VK_MULTIPLY, "NumMultiply" },
        { VirtualKey::NumSubtract, VK_SUBTRACT, "NumSubtract" },
        { VirtualKey::NumAdd,      VK_ADD,      "NumAdd"      },
        { VirtualKey::NumEnter,    VK_RETURN,   "NumEnter"    },
// -------------------------------------------------------------------------------------------------------------
        // TODO: Use scan codes to differentiate
        { VirtualKey::LeftShift,    VK_LSHIFT,   "LeftShift"    },
        { VirtualKey::LeftControl,  VK_LCONTROL, "LeftControl"  },
        { VirtualKey::LeftAlt,      VK_LMENU,    "LeftAlt"      },
        { VirtualKey::RightShift,   VK_RSHIFT,   "RightShift"   },
        { VirtualKey::RightControl, VK_RCONTROL, "RightControl" },
        { VirtualKey::RightAlt,     VK_RMENU,    "RightAlt"     },
        { VirtualKey::RightSuper,   VK_RWIN,     "RightSuper"   },
        { VirtualKey::Shift,        VK_SHIFT,    "Shift"        },
        { VirtualKey::Control,      VK_CONTROL,  "Control"      },
        { VirtualKey::Alt,          VK_MENU,     "Alt"          },
        { VirtualKey::Super,        VK_LWIN,     "Super"        },
// -------------------------------------------------------------------------------------------------------------
        { VirtualKey::VolumeMute,     VK_VOLUME_MUTE,      "VolumeMute"     },
        { VirtualKey::VolumeUp,       VK_VOLUME_UP,        "VolumeUp"       },
        { VirtualKey::VolumeDown,     VK_VOLUME_DOWN,      "VolumeDown"     },
        { VirtualKey::MediaStop,      VK_MEDIA_STOP,       "MediaStop"      },
        { VirtualKey::MediaPlayPause, VK_MEDIA_PLAY_PAUSE, "MediaPlayPause" },
        { VirtualKey::MediaPrev,      VK_MEDIA_PREV_TRACK, "MediaPrev"      },
        { VirtualKey::MediaNext,      VK_MEDIA_NEXT_TRACK, "MediaNext"      },
// -------------------------------------------------------------------------------------------------------------
        { VirtualKey::_0, 0x30 + 0, "0" },
        { VirtualKey::_1, 0x30 + 1, "1" },
        { VirtualKey::_2, 0x30 + 2, "2" },
        { VirtualKey::_3, 0x30 + 3, "3" },
        { VirtualKey::_4, 0x30 + 4, "4" },
        { VirtualKey::_5, 0x30 + 5, "5" },
        { VirtualKey::_6, 0x30 + 6, "6" },
        { VirtualKey::_7, 0x30 + 7, "7" },
        { VirtualKey::_8, 0x30 + 8, "8" },
        { VirtualKey::_9, 0x30 + 9, "9" },
// -------------------------------------------------------------------------------------------------------------
        { VirtualKey::A, int('A'), "A" },
        { VirtualKey::B, int('B'), "B" },
        { VirtualKey::C, int('C'), "C" },
        { VirtualKey::D, int('D'), "D" },
        { VirtualKey::E, int('E'), "E" },
        { VirtualKey::F, int('F'), "F" },
        { VirtualKey::G, int('G'), "G" },
        { VirtualKey::H, int('H'), "H" },
        { VirtualKey::I, int('I'), "I" },
        { VirtualKey::J, int('J'), "J" },
        { VirtualKey::K, int('K'), "K" },
        { VirtualKey::L, int('L'), "L" },
        { VirtualKey::M, int('M'), "M" },
        { VirtualKey::N, int('N'), "N" },
        { VirtualKey::O, int('O'), "O" },
        { VirtualKey::P, int('P'), "P" },
        { VirtualKey::Q, int('Q'), "Q" },
        { VirtualKey::R, int('R'), "R" },
        { VirtualKey::S, int('S'), "S" },
        { VirtualKey::T, int('T'), "T" },
        { VirtualKey::U, int('U'), "U" },
        { VirtualKey::V, int('V'), "V" },
        { VirtualKey::W, int('W'), "W" },
        { VirtualKey::X, int('X'), "X" },
        { VirtualKey::Y, int('Y'), "Y" },
        { VirtualKey::Z, int('Z'), "Z" },
// -------------------------------------------------------------------------------------------------------------
        { VirtualKey::F1,  VK_F1,  "F1"  },
        { VirtualKey::F2,  VK_F2,  "F2"  },
        { VirtualKey::F3,  VK_F3,  "F3"  },
        { VirtualKey::F4,  VK_F4,  "F4"  },
        { VirtualKey::F5,  VK_F5,  "F5"  },
        { VirtualKey::F6,  VK_F6,  "F6"  },
        { VirtualKey::F7,  VK_F7,  "F7"  },
        { VirtualKey::F8,  VK_F8,  "F8"  },
        { VirtualKey::F9,  VK_F9,  "F9"  },
        { VirtualKey::F10, VK_F10, "F10" },
        { VirtualKey::F11, VK_F11, "F11" },
        { VirtualKey::F12, VK_F12, "F12" },
        { VirtualKey::F13, VK_F13, "F13" },
        { VirtualKey::F14, VK_F14, "F14" },
        { VirtualKey::F15, VK_F15, "F15" },
        { VirtualKey::F16, VK_F16, "F16" },
        { VirtualKey::F17, VK_F17, "F17" },
        { VirtualKey::F18, VK_F18, "F18" },
        { VirtualKey::F19, VK_F19, "F19" },
        { VirtualKey::F20, VK_F20, "F20" },
        { VirtualKey::F21, VK_F21, "F21" },
        { VirtualKey::F22, VK_F22, "F22" },
        { VirtualKey::F23, VK_F23, "F23" },
        { VirtualKey::F24, VK_F24, "F24" },
    });

    void Application::Impl::InitMappings()
    {
        u32 max_win_key = 0;
        u32 max_nova_key = 0;
        for (auto& mapping : Win32VirtualKeyMappings) {
            max_win_key = std::max(max_win_key, mapping.win32_mapping);
            max_nova_key = std::max(max_nova_key, u32(mapping.nova_mapping));
        }

        win32_input.from_win32_virtual_key.resize(max_win_key + 1,  u32(VirtualKey::Unknown));
        win32_input.to_win32_virtual_key.resize(max_nova_key + 1, 0);
        win32_input.key_names.resize(max_nova_key + 1, "Unknown"sv);

        for (auto& mapping : Win32VirtualKeyMappings) {
            win32_input.from_win32_virtual_key[mapping.win32_mapping] = u32(mapping.nova_mapping);
            win32_input.to_win32_virtual_key[u32(mapping.nova_mapping)] = mapping.win32_mapping;
            win32_input.key_names[u32(mapping.nova_mapping)] = mapping.name;
        }
    }

    VirtualKey Application::ToVirtualKey(InputChannel channel) const
    {
        return channel.code < impl->win32_input.from_win32_virtual_key.size()
            ? VirtualKey(impl->win32_input.from_win32_virtual_key[channel.code])
            : VirtualKey::Unknown;
    }

    static
    u32 ToWin32VirtualKey(Application app, VirtualKey key)
    {
        return app->win32_input.to_win32_virtual_key[u32(key)];
    }

    std::string_view Application::VirtualKeyToString(VirtualKey key) const
    {
        return impl->win32_input.key_names[u32(key)];
    }

    bool Application::IsVirtualKeyDown(VirtualKey button) const
    {
        auto code = ToWin32VirtualKey(*this, button);
        if (!code) return false;

        return GetKeyState(code) & 0x8000;
    }

    InputChannelState Application::InputState(InputChannel channel) const
    {
        // TODO: Handle non virtual key categories

        auto state = GetKeyState(channel.code);
        bool pressed = state & 0x8000;
        bool toggled = state & 0x0001;

        return {
            .channel = channel,
            .pressed = pressed,
            .toggled = toggled,
            .value = pressed ? 1.f : 0.f
        };
    }
}