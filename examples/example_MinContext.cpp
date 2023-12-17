#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/rhi/nova_RHI.hpp>

NOVA_EXAMPLE(MinContext, "min-context")
{
    auto context = nova::Context::Create({
        .debug = false,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    std::cin.get();
}