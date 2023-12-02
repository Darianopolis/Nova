#include "nova_Win32Window.hpp"

namespace nova
{
    VirtualKey Application::ToVirtualKey(InputChannel channel) const
    {
        switch (channel.code)
        {
            break;case VK_LBUTTON: return VirtualKey::MousePrimary;
            break;case VK_RBUTTON: return VirtualKey::MouseSecondary;
            break;case VK_MBUTTON: return VirtualKey::MouseMiddle;

            break;case VK_TAB: return VirtualKey::Tab;

            break;case VK_LEFT:  return VirtualKey::Left;
            break;case VK_RIGHT: return VirtualKey::Right;
            break;case VK_UP:    return VirtualKey::Up;
            break;case VK_DOWN:  return VirtualKey::Down;

            break;case VK_PRIOR:  return VirtualKey::PageUp;
            break;case VK_NEXT:   return VirtualKey::PageDown;
            break;case VK_HOME:   return VirtualKey::Home;
            break;case VK_END:    return VirtualKey::End;
            break;case VK_INSERT: return VirtualKey::Insert;
            break;case VK_DELETE: return VirtualKey::Delete;
            break;case VK_BACK:   return VirtualKey::Backspace;
            break;case VK_SPACE:  return VirtualKey::Space;
            break;case VK_RETURN: return VirtualKey::Enter;
            break;case VK_ESCAPE: return VirtualKey::Escape;

            break;case VK_SHIFT:   return VirtualKey::LeftShift;
            break;case VK_CONTROL: return VirtualKey::LeftControl;
            break;case VK_MENU:    return VirtualKey::LeftAlt;

            break;case VK_LSHIFT:   return VirtualKey::LeftShift;
            break;case VK_LCONTROL: return VirtualKey::LeftControl;
            break;case VK_LMENU:    return VirtualKey::LeftAlt;
            break;case VK_LWIN:     return VirtualKey::LeftSuper;

            break;case VK_RSHIFT:   return VirtualKey::RightShift;
            break;case VK_RCONTROL: return VirtualKey::RightControl;
            break;case VK_RMENU:    return VirtualKey::RightAlt;
            break;case VK_RWIN:     return VirtualKey::RightSuper;
        }

        return VirtualKey::Unknown;
    }

    static
    u32 GetVirtualKeyForButton(VirtualKey button)
    {
        switch (button)
        {
            break;case VirtualKey::MousePrimary:   return VK_LBUTTON;
            break;case VirtualKey::MouseSecondary: return VK_RBUTTON;
            break;case VirtualKey::MouseMiddle:    return VK_MBUTTON;

            break;case VirtualKey::Tab: return VK_TAB;

            break;case VirtualKey::Left:  return VK_LEFT;
            break;case VirtualKey::Right: return VK_RIGHT;
            break;case VirtualKey::Up:    return VK_UP;
            break;case VirtualKey::Down:  return VK_DOWN;

            break;case VirtualKey::PageUp:    return VK_PRIOR;
            break;case VirtualKey::PageDown:  return VK_NEXT;
            break;case VirtualKey::Home:      return VK_HOME;
            break;case VirtualKey::End:       return VK_END;
            break;case VirtualKey::Insert:    return VK_INSERT;
            break;case VirtualKey::Delete:    return VK_DELETE;
            break;case VirtualKey::Backspace: return VK_BACK;
            break;case VirtualKey::Space:     return VK_SPACE;
            break;case VirtualKey::Enter:     return VK_RETURN;
            break;case VirtualKey::Escape:    return VK_ESCAPE;

            break;case VirtualKey::LeftShift:   return VK_LSHIFT;
            break;case VirtualKey::LeftControl: return VK_LCONTROL;
            break;case VirtualKey::LeftAlt:     return VK_LMENU;
            break;case VirtualKey::LeftSuper:   return VK_LWIN;

            break;case VirtualKey::RightShift:   return VK_RSHIFT;
            break;case VirtualKey::RightControl: return VK_RCONTROL;
            break;case VirtualKey::RightAlt:     return VK_RMENU;
            break;case VirtualKey::RightSuper:   return VK_RWIN;
        }

        return 0;
    }

    bool Application::IsVirtualKeyDown(VirtualKey button) const
    {
        auto code = GetVirtualKeyForButton(button);
        if (!code) return false;

        return GetKeyState(code) & 0x8000;
    }

    InputChannelState Application::GetInputState(InputChannel channel) const
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