#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/rhi/nova_RHI.hpp>

#include <nova/window/nova_Window.hpp>

NOVA_EXAMPLE(MinContext, "min-context")
{
    auto context = nova::Context::Create({
        .debug = false,
        // .ray_tracing = true,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    std::cin.get();
}