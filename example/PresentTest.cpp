#include <nova/rhi/nova_RHI.hpp>

#include <nova/rhi/vulkan/nova_VulkanContext.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

void TryMain()
{
    std::unique_ptr<nova::Context> ctx = std::make_unique<nova::VulkanContext>(nova::ContextConfig {
        .debug = true,
    });

    glfwInit();
    NOVA_ON_SCOPE_EXIT(&) { glfwTerminate(); };
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto window = glfwCreateWindow(1920, 1200, "Present Test", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) { glfwDestroyWindow(window); };

    auto swapchain = ctx->Swapchain_Create(glfwGetWin32Window(window),
        nova::TextureUsage::ColorAttach | nova::TextureUsage::Storage,
        nova::PresentMode::Mailbox);
    NOVA_ON_SCOPE_EXIT(&) {
        ctx->WaitIdle();
        ctx->Destroy(swapchain);
    };

    [[maybe_unused]] auto fence = ctx->Fence_Create();

    auto queue = ctx->Queue_Get(nova::QueueFlags::Graphics);

    while (!glfwWindowShouldClose(window))
    {
        ctx->Fence_Wait(fence);
        ctx->Queue_Acquire(queue, {swapchain}, {fence});
        ctx->Queue_Present(queue, {swapchain}, {fence});

        glfwWaitEvents();
    }
}

int main()
{
    try
    {
        TryMain();
    }
    catch(...)
    {

    }
}