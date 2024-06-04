#include "main/example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>
#include <nova/window/nova_Window.hpp>

NOVA_EXAMPLE(MinContext, "min-context")
{
    nova::rhi::Init({});

    // Minimal Window context

    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };

    nova::Window::Create(app)
        .SetTitle("Minimal Context")
        .SetSize({ 1920, 1080 }, nova::WindowPart::Client)
        .Show(true);

    while (app.ProcessEvents()) {
        app.WaitForEvents();
    }
}