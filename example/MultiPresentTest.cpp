#include <nova/rhi/nova_RHI.hpp>
#include <nova/core/nova_Timer.hpp>
#include <nova/imgui/nova_ImGui.hpp>

using namespace nova::types;

int main()
{
    NOVA_LOGEXPR(mi_version());

    auto context = nova::Context::Create({
        .debug = false,
    });
    NOVA_ON_SCOPE_EXIT(&) { nova::Context::Destroy(context); };

    auto presentMode = nova::PresentMode::Fifo;
    auto swapchainUsage = nova::TextureUsage::TransferDst
        | nova::TextureUsage::ColorAttach
        | nova::TextureUsage::Storage;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto window = glfwCreateWindow(1920, 1200, "Present Test Window #1", nullptr, nullptr);
    nova::Surface     surface(context, glfwGetWin32Window(window));
    nova::Swapchain swapchain(context, &surface, swapchainUsage, presentMode);

    auto window2 = glfwCreateWindow(1920, 1200, "Present Test Window #2", nullptr, nullptr);
    nova::Surface     surface2(context, glfwGetWin32Window(window2));
    nova::Swapchain swapchain2(context, &surface2, swapchainUsage, presentMode);

    auto queue = context->graphics;

    auto tracker = context->CreateResourceTracker();

    nova::Fence*             fences[] = { context->CreateFence(),    context->CreateFence()    };
    nova::CommandPool* commandPools[] = { context->CreateCommandPool(), context->CreateCommandPool() };

    nova::ImGuiWrapper* imgui;
    {
        auto cmd = commandPools[0]->BeginPrimary(tracker);
        imgui = nova::ImGuiWrapper::Create(context, cmd, &swapchain, window, ImGuiConfigFlags_ViewportsEnable);
        queue->Submit({cmd}, {}, {fences[0]});
        fences[0]->Wait();
    }

    u64 frame = 0;
    auto lastTime = std::chrono::steady_clock::now();
    auto frames = 0;
    auto update = [&] {

        // Debug output statistics
        frames++;
        auto newTime = std::chrono::steady_clock::now();
        if (newTime - lastTime > 1s)
        {
            NOVA_LOG("\nFps = {}\nAllocations = {:3} (+ {} /s)", frames, nova::Context::AllocationCount.load(), nova::Context::NewAllocationCount.exchange(0));

            NOVA_LOG("submit :: clear     = {}\nsubmit :: adapting1 = {}\nsubmit :: adapting2 = {}\npresent             = {}",
                nova::submitting.exchange(0) / frames,
                nova::adapting1.exchange(0)  / frames,
                nova::adapting2.exchange(0)  / frames,
                nova::presenting.exchange(0) / frames);

            lastTime = std::chrono::steady_clock::now();
            frames = 0;
        }

        // Pick fence and commandPool for frame in flight
        auto fence = fences[frame % 2];
        auto commandPool = commandPools[frame % 2];
        frame++;

        // Wait for previous commands in frame to complete
        fence->Wait();

        // Acquire new images from swapchains
        queue->Acquire({&swapchain, &swapchain2}, {fence});

        // Clear resource state tracking
        tracker->Clear(3);

        // Reset command pool and begin new command list
        commandPool->Reset();
        auto cmd = commandPool->BeginPrimary(tracker);

        // Clear screen
        cmd->Clear(swapchain.current, Vec4(26 / 255.f, 89 / 255.f, 71 / 255.f, 1.f));

        // Draw ImGui demo window
        imgui->BeginFrame();
        ImGui::ShowDemoWindow();
        imgui->EndFrame(cmd, &swapchain);

        // Present #1
        cmd->Present(swapchain.current);

        // Clear and present #2
        cmd->Clear(swapchain2.current, Vec4(112 / 255.f, 53 / 255.f, 132 / 255.f, 1.f));
        cmd->Present(swapchain2.current);

        // Submit work
        queue->Submit({cmd}, {fence}, {fence});

        // Present both swapchains
        queue->Present({&swapchain, &swapchain2}, {fence}, false);
    };

    glfwSetWindowUserPointer(window, &update);
    glfwSetWindowSizeCallback(window, [](auto w, int,int) {
        (*(decltype(update)*)glfwGetWindowUserPointer(w))();
    });

    glfwSetWindowUserPointer(window2, &update);
    glfwSetWindowSizeCallback(window2, [](auto w, int,int) {
        (*(decltype(update)*)glfwGetWindowUserPointer(w))();
    });

    while (!glfwWindowShouldClose(window) && !glfwWindowShouldClose(window2))
    {
        update();
        glfwPollEvents();
    }

    fences[0]->Wait();
    fences[1]->Wait();
    context->DestroyFence(fences[0]);
    context->DestroyFence(fences[1]);
    context->DestroyCommandPool(commandPools[0]);
    context->DestroyCommandPool(commandPools[1]);
    context->DestroyResourceTracker(tracker);

    nova::ImGuiWrapper::Destroy(imgui);

    glfwDestroyWindow(window);
    glfwDestroyWindow(window2);

    glfwTerminate();

    NOVA_LOG("Allocations = {}", nova::Context::AllocationCount.load());
}