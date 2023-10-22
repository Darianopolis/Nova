#include "nova_Window.hpp"

#include <GLFW/glfw3native.h>

namespace nova
{
    Window* Window::Create(const WindowConfig& config)
    {
        NOVA_DO_ONCE() { glfwInit(); };
        NOVA_ON_EXIT() { glfwTerminate(); };

        auto window = new Window;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window->window = glfwCreateWindow(
            i32(config.size.x), i32(config.size.y),
            config.title.c_str(),
            nullptr, nullptr);

        return window;
    }

    bool Window::ShouldClose()
    {
        return glfwWindowShouldClose(window);
    }

    void Window::Destroy(Window* window)
    {
        glfwDestroyWindow(window->window);
    }

    void* Window::GetNativeHandle()
    {
        return glfwGetWin32Window(window);
    }

    Vec2 Window::GetCursorPos()
    {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        return Vec2(f32(x), f32(y));
    }

    Vec2U Window::GetClientSize()
    {
        int x, y;
        glfwGetFramebufferSize(window, &x, &y);
        return Vec2U(u32(x), u32(y));
    }
}