#include <nova/window/nova_Window.hpp>
#include <nova/rhi/nova_RHI.hpp>
#include <nova/core/nova_Timer.hpp>

#include <nova/imgui/nova_ImGui.hpp>

using namespace nova::types;

int main()
{
    NOVA_LOGEXPR(mi_version());

    auto context = nova::Context::Create(true);

    auto presentMode = nova::PresentMode::Mailbox;
    // auto presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    // auto presentMode = VK_PRESENT_MODE_FIFO_KHR;

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

    auto hostFence = context->CreateFence();

    u64 frame = 0;

    nova::Timer timer;

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
        queue->Acquire({swapchain, swapchain2}, {fence});

        commandPool->Reset();
        auto cmd = commandPool->BeginPrimary(tracker);

        cmd->Clear(swapchain->image, Vec4(26 / 255.f, 89 / 255.f, 71 / 255.f, 1.f));

        imgui->BeginFrame();
        ImGui::ShowDemoWindow();
        imgui->EndFrame(cmd, swapchain);

        cmd->Transition(swapchain->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        cmd->Clear(swapchain2->image, Vec4(112 / 255.f, 53 / 255.f, 132 / 255.f, 1.f));
        cmd->Transition(swapchain2->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        // auto value = hostFence->Advance();
        // std::thread([=] {
        //     std::this_thread::sleep_for(5ms);
        //     hostFence->Signal(value);
        // }).detach();

        // NOVA_TIMEIT_RESET();
        // std::this_thread::sleep_for(5ms);
        // NOVA_TIMEIT("sleep");

        queue->Submit({cmd}, {hostFence, fence}, {fence});
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

    auto lastFrame = std::chrono::steady_clock::now();
    while (window->PollEvents() && window2->PollEvents())
    {
        // while (timer.Wait(lastFrame + (1000ms / 120.0) - std::chrono::steady_clock::now(), true))
        //     std::this_thread::yield();
        // lastFrame = std::chrono::steady_clock::now();
        update();
    }

    fences[0]->Wait();
    fences[1]->Wait();
    context->DestroyFence(fences[0]);
    context->DestroyFence(fences[1]);
    context->DestroyFence(hostFence);
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