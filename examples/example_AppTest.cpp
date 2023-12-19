#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/window/nova_Window.hpp>

NOVA_EXAMPLE(AppTest, "app")
{
    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };

     auto window = nova::Window::Create(app, {
        .title = "Application Test",
        .size = { 1920, 1080 },
    });

    while (app.ProcessEvents()) {
        app.WaitForEvents();
    }
}