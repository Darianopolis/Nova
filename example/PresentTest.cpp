#include <nova/rhi/nova_RHI.hpp>

#include <nova/rhi/vulkan/nova_VulkanContext.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

using namespace nova::types;

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

    auto queue = ctx->Queue_Get(nova::QueueFlags::Graphics);
    auto fence = ctx->Fence_Create();
    auto cmdState = ctx->Commands_CreateState();
    auto cmdPool = ctx->Commands_CreatePool(queue);

    while (!glfwWindowShouldClose(window))
    {
        ctx->Fence_Wait(fence);
        ctx->Queue_Acquire(queue, {swapchain}, {fence});

        ctx->Commands_Reset(cmdPool);
        auto cmd = ctx->Commands_Begin(cmdPool, cmdState);

        ctx->Cmd_Clear(cmd, ctx->Swapchain_GetCurrent(swapchain),
            Vec4(33 / 255.f, 81 / 255.f, 68 / 255.f, 1.f));

        ctx->Cmd_Present(cmd, swapchain);
        ctx->Queue_Submit(queue, {cmd}, {fence}, {fence});
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
        NOVA_LOG("Exiting on exception...");
    }
}