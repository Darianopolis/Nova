#include <nova/rhi/nova_RHI.hpp>
#include <nova/core/nova_Timer.hpp>
#include <nova/imgui/nova_ImGui.hpp>

using namespace nova::types;

int main()
{
    auto context = nova::Context({ .debug = false });

    auto presentMode = nova::PresentMode::Immediate;
    auto swapchainUsage = nova::TextureUsage::TransferDst
        | nova::TextureUsage::ColorAttach
        | nova::TextureUsage::Storage;

    glfwInit();
    NOVA_ON_SCOPE_EXIT(&) { glfwTerminate(); };
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto window1 = glfwCreateWindow(1920, 1200, "Present Test Window #1", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) { glfwDestroyWindow(window1); };
    auto surface1 = nova::Surface(context, glfwGetWin32Window(window1));
    auto swapchain = nova::Swapchain(context, surface1, swapchainUsage, presentMode);

    auto window2 = glfwCreateWindow(1920, 1200, "Present Test Window #2", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) { glfwDestroyWindow(window2); };
    auto surface2 = nova::Surface(context, glfwGetWin32Window(window2));
    auto swapchain2 = nova::Swapchain(context, surface2, swapchainUsage, presentMode);

    auto queue = context.GetQueue(nova::QueueFlags::Graphics);
    auto tracker = nova::ResourceTracker(context);
    nova::Fence fences[] { {context}, {context} };
    nova::CommandPool commandPools[] { {context, queue}, {context, queue} };

    auto cmd = commandPools[0].Begin(tracker);
    auto imgui = nova::ImGuiWrapper(context, cmd, swapchain, window1, ImGuiConfigFlags_ViewportsEnable);
    queue.Submit({cmd}, {}, {fences[0]});
    fences[0].Wait();

    u64 frame = 0;
    auto lastTime = std::chrono::steady_clock::now();
    auto frames = 0;
    auto update = [&] {

        // Debug output statistics
        frames++;
        auto newTime = std::chrono::steady_clock::now();
        if (newTime - lastTime > 1s)
        {
            NOVA_LOG("\nFps = {}\nAllocations = {:3} (+ {} /s)",
                frames, nova::ContextImpl::AllocationCount.load(), nova::ContextImpl::NewAllocationCount.exchange(0));
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
        fence.Wait();

        // Acquire new images from swapchains
        queue.Acquire({swapchain, swapchain2}, {fence});

        // Clear resource state tracking
        tracker.Clear(3);

        // Reset command pool and begin new command list
        commandPool.Reset();
        cmd = commandPool.Begin(tracker);

        // Clear screen
        cmd.Clear(swapchain.GetCurrent(), Vec4(26 / 255.f, 89 / 255.f, 71 / 255.f, 1.f));

        // Draw ImGui demo window
        imgui.BeginFrame();
        ImGui::ShowDemoWindow();
        imgui.EndFrame(cmd, swapchain);

        // Present #1
        cmd.Present(swapchain.GetCurrent());

        // Clear and present #2
        cmd.Clear(swapchain2.GetCurrent(), Vec4(112 / 255.f, 53 / 255.f, 132 / 255.f, 1.f));
        cmd.Present(swapchain2.GetCurrent());

        // Submit work
        queue.Submit({cmd}, {fence}, {fence});

        // Present both swapchains
        queue.Present({swapchain, swapchain2}, {fence}, false);
    };

    glfwSetWindowUserPointer(window1, &update);
    glfwSetWindowSizeCallback(window1, [](auto w, int,int) {
        (*(decltype(update)*)glfwGetWindowUserPointer(w))();
    });

    glfwSetWindowUserPointer(window2, &update);
    glfwSetWindowSizeCallback(window2, [](auto w, int,int) {
        (*(decltype(update)*)glfwGetWindowUserPointer(w))();
    });

    NOVA_ON_SCOPE_EXIT(&) {
        fences[0].Wait();
        fences[1].Wait();
    };
    while (!glfwWindowShouldClose(window1) && !glfwWindowShouldClose(window2))
    {
        update();
        glfwPollEvents();
    }

    NOVA_LOG("Allocations = {}", nova::ContextImpl::AllocationCount.load());
}