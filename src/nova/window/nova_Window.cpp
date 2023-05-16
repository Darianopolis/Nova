#include "nova_Window.hpp"

namespace nova
{
    Window* Window::Create()
    {
        NOVA_DO_ONCE() { glfwInit(); };
        NOVA_ON_EXIT() { glfwTerminate(); };

        auto window = new Window;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window->window = glfwCreateWindow(1920, 1200, "test", nullptr, nullptr);

        return window;
    }

    bool Window::PollEvents()
    {
        if (glfwWindowShouldClose(window))
            return false;

        glfwPollEvents();
        return true;
    }

    bool Window::WaitEvents()
    {
        if (glfwWindowShouldClose(window))
            return false;

        glfwWaitEvents();
        return true;
    }

    Window::~Window()
    {
        glfwDestroyWindow(window);
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