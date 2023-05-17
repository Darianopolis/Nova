#include <nova/window/nova_Window.hpp>
#include <nova/rhi/nova_RHI.hpp>

using namespace nova::types;

int main()
{
    auto context = nova::Context::Create(false);

    auto window = nova::Window::Create();
    auto surface = context->CreateSurface(glfwGetWin32Window(window->window));
    auto swapchain = context->CreateSwapchain(surface,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT
        | VK_IMAGE_USAGE_STORAGE_BIT,
        VK_PRESENT_MODE_FIFO_KHR);

    auto queue = context->graphics;

    nova::Fence*             fences[] = { context->CreateFence(),    context->CreateFence()    };
    nova::CommandPool* commandPools[] = { context->CreateCommands(), context->CreateCommands() };

    u64 frame = 0;

    auto last = std::chrono::steady_clock::now();
    auto frames = 0;
    auto update = [&] {
        frames++;
        if (std::chrono::steady_clock::now() - last > 1s)
        {
            NOVA_LOG("Allocations = {} Fps = {:3} New = {}", nova::Context::AllocationCount.load(), frames, nova::Context::NewAllocationCount.exchange(0));
            last = std::chrono::steady_clock::now();
            frames = 0;
        }

        auto fence = fences[frame % 2];
        auto commandPool = commandPools[frame % 2];
        frame++;

        fence->Wait();
        queue->Acquire(swapchain, fence);

        commandPool->Reset();
        auto cmd = commandPool->Begin();

        cmd->Clear(swapchain->image, Vec4(26 / 255.f, 89 / 255.f, 71 / 255.f, 1.f));
        cmd->Transition(swapchain->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        queue->Submit(cmd, fence, fence);
        queue->Present(swapchain, fence);
    };

    glfwSetWindowUserPointer(window->window, &update);
    glfwSetWindowSizeCallback(window->window, [](auto w, int,int) {
        (*(decltype(update)*)glfwGetWindowUserPointer(w))();
    });

    while (window->PollEvents())
        update();

    fences[0]->Wait();
    fences[1]->Wait();
    context->Destroy(fences[0], fences[1]);
    context->Destroy(commandPools[0], commandPools[1]);
    context->Destroy(swapchain, surface);
    nova::Context::Destroy(context);
    nova::Window::Destroy(window);

    NOVA_LOG("Allocations = {}", nova::Context::AllocationCount.load());
}