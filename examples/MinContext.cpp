#include "main/Main.hpp"

#include <nova/gpu/RHI.hpp>
#include <nova/window/Window.hpp>

NOVA_EXAMPLE(MinContext, "min-context")
{
    // Minimal RHI context

    auto context = nova::Context::Create({ .debug = true });
    NOVA_DEFER(&) { context.Destroy(); };

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
