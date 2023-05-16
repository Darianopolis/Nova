#include <nova/window/nova_Window.hpp>
#include <nova/rhi/nova_RHI.hpp>

using namespace nova::types;

int main()
{
    auto context = nova::Context::Create(true);
    auto window = nova::Window::Create();

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(context->instance, window->window, context->pAlloc, &surface);

    auto swapchain = context->CreateSwapchain(surface,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT
        | VK_IMAGE_USAGE_STORAGE_BIT,
        VK_PRESENT_MODE_FIFO_KHR);

    auto fence = context->fence;
    auto commands = context->commands;
    auto& cmd = context->cmd;

    while (window->WaitEvents())
    {
        fence->Wait();
        context->Acquire(*swapchain, context->graphics, fence.Raw());
        commands->Clear();
        cmd = commands->Allocate();

        commands->ClearColor(cmd, *swapchain->image, Vec4(1.f, 0.f, 0.f, 1.f));

        context->Transition(cmd, *swapchain->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        commands->Submit(cmd, fence.Raw(), fence.Raw());
        context->Present(*swapchain, fence.Raw());
    }

    fence->Wait();
    vkDeviceWaitIdle(context->device);
}