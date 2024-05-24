#include "main/example_Main.hpp"

#include <nova/window/nova_Window.hpp>

NOVA_EXAMPLE(AppTest, "app")
{
    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };

    nova::Window::Create(app)
        .SetTitle("Application Test")
        .Show(true);

    while (app.ProcessEvents()) {
        app.WaitForEvents();
    }
}