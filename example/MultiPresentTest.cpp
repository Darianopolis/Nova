#include <nova/window/nova_Window.hpp>
#include <nova/rhi/nova_RHI.hpp>
#include <nova/core/nova_Timer.hpp>

#include <nova/imgui/nova_ImGui.hpp>

using namespace nova::types;

int main()
{
    NOVA_LOGEXPR(mi_version());

    auto context = nova::Context::Create(true);

    auto presentMode = nova::PresentMode::Fifo;

    auto window = nova::Window::Create();
    auto surface = context->CreateSurface(glfwGetWin32Window(window->window));
    auto swapchain = context->CreateSwapchain(surface,
        nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach
        | nova::ImageUsage::Storage,
        presentMode);

    auto window2 = nova::Window::Create();
    auto surface2 = context->CreateSurface(glfwGetWin32Window(window2->window));
    auto swapchain2 = context->CreateSwapchain(surface2,
        nova::ImageUsage::TransferDst
        | nova::ImageUsage::Storage,
        presentMode);

    auto queue = context->graphics;

    auto tracker = context->CreateResourceTracker();

    nova::Fence*             fences[] = { context->CreateFence(),    context->CreateFence()    };
    nova::CommandPool* commandPools[] = { context->CreateCommandPool(), context->CreateCommandPool() };

    nova::ImGuiWrapper* imgui;
    {
        auto cmd = commandPools[0]->BeginPrimary(tracker);
        imgui = nova::ImGuiWrapper::Create(context, cmd, swapchain, window->window, ImGuiConfigFlags_ViewportsEnable);
        queue->Submit({cmd}, {}, {fences[0]});
        fences[0]->Wait();
    }

    u64 frame = 0;
    auto last = std::chrono::steady_clock::now();
    auto frames = 0;
    auto update = [&] {

        // Debug output statistics
        frames++;
        if (std::chrono::steady_clock::now() - last > 1s)
        {
            NOVA_LOG("Allocations = {} Fps = {:3} New = {}", nova::Context::AllocationCount.load(), frames, nova::Context::NewAllocationCount.exchange(0));
            last = std::chrono::steady_clock::now();
            frames = 0;
        }

        // Pick fence and commandPool for frame in flight
        auto fence = fences[frame % 2];
        auto commandPool = commandPools[frame % 2];
        frame++;

        // Wait for previous commands in frame to complete
        fence->Wait();

        // Acquire new images from swapchains
        queue->Acquire({swapchain, swapchain2}, {fence});

        // Clear resource state tracking
        tracker->Clear(3);

        // Reset command pool and begin new command list
        commandPool->Reset();
        auto cmd = commandPool->BeginPrimary(tracker);

        // Clear screen
        cmd->Clear(swapchain->image, Vec4(26 / 255.f, 89 / 255.f, 71 / 255.f, 1.f));

        // Draw ImGui demo window
        imgui->BeginFrame();
        ImGui::ShowDemoWindow();
        imgui->EndFrame(cmd, swapchain);

        // Transition ready for present
        cmd->Transition(swapchain->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_NONE, 0);

        // Clear second screen and transition
        cmd->Clear(swapchain2->image, Vec4(112 / 255.f, 53 / 255.f, 132 / 255.f, 1.f));
        cmd->Transition(swapchain2->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_NONE, 0);

        // Submit work
        queue->Submit({cmd}, {fence}, {fence});

        // Present both swapchains
        queue->Present({swapchain, swapchain2}, {fence}, true);
    };

    glfwSetWindowUserPointer(window->window, &update);
    glfwSetWindowSizeCallback(window->window, [](auto w, int,int) {
        (*(decltype(update)*)glfwGetWindowUserPointer(w))();
    });

    glfwSetWindowUserPointer(window2->window, &update);
    glfwSetWindowSizeCallback(window2->window, [](auto w, int,int) {
        (*(decltype(update)*)glfwGetWindowUserPointer(w))();
    });

    while (window->PollEvents() && window2->PollEvents())
        update();

    fences[0]->Wait();
    fences[1]->Wait();
    context->DestroyFence(fences[0]);
    context->DestroyFence(fences[1]);
    context->DestroyCommandPool(commandPools[0]);
    context->DestroyCommandPool(commandPools[1]);
    context->DestroyResourceTracker(tracker);
    context->DestroySwapchain(swapchain);
    context->DestroySwapchain(swapchain2);
    context->DestroySurface(surface);
    context->DestroySurface(surface2);

    nova::ImGuiWrapper::Destroy(imgui);

    nova::Window::Destroy(window);
    nova::Window::Destroy(window2);

    nova::Context::Destroy(context);

    NOVA_LOG("Allocations = {}", nova::Context::AllocationCount.load());
}