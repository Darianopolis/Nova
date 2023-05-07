#include <VulkanBackend/VulkanBackend.hpp>

#include <Renderer/Renderer.hpp>
#include <ImGui/ImGuiBackend.h>

int main()
{
    auto ctx = pyr::CreateContext(true);
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1080, "test", nullptr, nullptr);

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(ctx.instance, window, nullptr, &surface);

    auto swapchain = ctx.CreateSwapchain(surface,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT
        | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        | VK_IMAGE_USAGE_STORAGE_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_PRESENT_MODE_MAILBOX_KHR);

    pyr::Renderer renderer;
    renderer.Init(ctx);

    pyr::ImGuiBackend imgui;
    imgui.Init(ctx, swapchain, window,
        ImGuiConfigFlags_ViewportsEnable
        | ImGuiConfigFlags_DockingEnable);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ctx.GetNextImage(swapchain);

        imgui.BeginFrame();
        ImGui::ShowDemoWindow();

        renderer.Draw(*swapchain.image);

        imgui.EndFrame(swapchain);
        ctx.Present(swapchain);
    }

    vkDeviceWaitIdle(ctx.device);

    glfwDestroyWindow(window);
    glfwTerminate();
}