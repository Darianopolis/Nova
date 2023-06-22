#include <nova/rhi/nova_RHI.hpp>

#include <nova/rhi/vulkan/nova_VulkanContext.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

void TryMain()
{
    auto ctx = std::make_unique<nova::VulkanContext>(nova::ContextConfig {
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
    NOVA_ON_SCOPE_EXIT(&) { ctx->Destroy(swapchain); };

    (void)swapchain;

    while (!glfwWindowShouldClose(window))
    {
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