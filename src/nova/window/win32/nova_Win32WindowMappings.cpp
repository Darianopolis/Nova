#include "nova_Win32Window.hpp"

namespace nova
{
    Button Application::GetButton(u64 code) const
    {
        switch (code)
        {
            break;case VK_LBUTTON: return Button::MousePrimary;
            break;case VK_RBUTTON: return Button::MouseSecondary;
            break;case VK_MBUTTON: return Button::MouseMiddle;

            break;case VK_TAB: return Button::Tab;

            break;case VK_LEFT:  return Button::Left;
            break;case VK_RIGHT: return Button::Right;
            break;case VK_UP:    return Button::Up;
            break;case VK_DOWN:  return Button::Down;

            break;case VK_PRIOR:  return Button::PageUp;
            break;case VK_NEXT:   return Button::PageDown;
            break;case VK_HOME:   return Button::Home;
            break;case VK_END:    return Button::End;
            break;case VK_INSERT: return Button::Insert;
            break;case VK_DELETE: return Button::Delete;
            break;case VK_BACK:   return Button::Backspace;
            break;case VK_SPACE:  return Button::Space;
            break;case VK_RETURN: return Button::Enter;
            break;case VK_ESCAPE: return Button::Escape;

            break;case VK_SHIFT:   return Button::LeftShift;
            break;case VK_CONTROL: return Button::LeftControl;
            break;case VK_MENU:    return Button::LeftAlt;

            break;case VK_LSHIFT:   return Button::LeftShift;
            break;case VK_LCONTROL: return Button::LeftControl;
            break;case VK_LMENU:    return Button::LeftAlt;
            break;case VK_LWIN:     return Button::LeftSuper;

            break;case VK_RSHIFT:   return Button::RightShift;
            break;case VK_RCONTROL: return Button::RightControl;
            break;case VK_RMENU:    return Button::RightAlt;
            break;case VK_RWIN:     return Button::RightSuper;
        }

        return Button::Unknown;
    }

    static
    u32 GetVirtualKeyForButton(Button button)
    {
        switch (button)
        {
            break;case Button::MousePrimary:   return VK_LBUTTON;
            break;case Button::MouseSecondary: return VK_RBUTTON;
            break;case Button::MouseMiddle:    return VK_MBUTTON;

            break;case Button::Tab: return VK_TAB;

            break;case Button::Left:  return VK_LEFT;
            break;case Button::Right: return VK_RIGHT;
            break;case Button::Up:    return VK_UP;
            break;case Button::Down:  return VK_DOWN;

            break;case Button::PageUp:    return VK_PRIOR;
            break;case Button::PageDown:  return VK_NEXT;
            break;case Button::Home:      return VK_HOME;
            break;case Button::End:       return VK_END;
            break;case Button::Insert:    return VK_INSERT;
            break;case Button::Delete:    return VK_DELETE;
            break;case Button::Backspace: return VK_BACK;
            break;case Button::Space:     return VK_SPACE;
            break;case Button::Enter:     return VK_RETURN;
            break;case Button::Escape:    return VK_ESCAPE;

            break;case Button::LeftShift:   return VK_LSHIFT;
            break;case Button::LeftControl: return VK_LCONTROL;
            break;case Button::LeftAlt:     return VK_LMENU;
            break;case Button::LeftSuper:   return VK_LWIN;

            break;case Button::RightShift:   return VK_RSHIFT;
            break;case Button::RightControl: return VK_RCONTROL;
            break;case Button::RightAlt:     return VK_RMENU;
            break;case Button::RightSuper:   return VK_RWIN;
        }

        return 0;
    }

    bool Application::IsButtonDown(Button button) const
    {
        auto code = GetVirtualKeyForButton(button);
        if (!code) return false;

        return GetKeyState(code) & 0x8000;
    }
}