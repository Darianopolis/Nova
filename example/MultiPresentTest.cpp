#include <nova/rhi/nova_RHI.hpp>
#include <nova/core/nova_Timer.hpp>
#include <nova/imgui/nova_ImGui.hpp>

// For internal allocation tracking
// TODO: Make this public statistics!
#include <nova/rhi/nova_RHI_Impl.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

using namespace nova::types;

int main()
{
    auto context = +nova::Context({ .debug = true });

    auto presentMode = nova::PresentMode::Mailbox;
    auto swapchainUsage = nova::TextureUsage::ColorAttach
        | nova::TextureUsage::Storage;

    glfwInit();
    NOVA_ON_SCOPE_EXIT(&) { glfwTerminate(); };
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto window1 = glfwCreateWindow(1920, 1200, "Present Test Window #1", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) { glfwDestroyWindow(window1); };
    auto surface1 = +nova::Surface(context, glfwGetWin32Window(window1));
    auto swapchain1 = +nova::Swapchain(context, surface1, swapchainUsage, presentMode);

    auto window2 = glfwCreateWindow(1920, 1200, "Present Test Window #2", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) { glfwDestroyWindow(window2); };
    auto surface2 = +nova::Surface(context, glfwGetWin32Window(window2));
    auto swapchain2 = +nova::Swapchain(context, surface2, swapchainUsage, presentMode);

    auto queue = context.GetQueue(nova::QueueFlags::Graphics);
    auto state = +nova::CommandState(context);
    nova::Fence::Arc fences[] { +nova::Fence{context}, +nova::Fence{context} };
    nova::CommandPool::Arc commandPools[] { +nova::CommandPool{context, queue}, +nova::CommandPool{context, queue} };

    auto cmd = commandPools[0].Begin(state);
    auto imgui = +nova::ImGuiWrapper(context, cmd, swapchain1.GetFormat(), window1, { .flags = ImGuiConfigFlags_ViewportsEnable });
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
            f64 divisor = 1000.0 * frames;
            NOVA_LOG("submit :: clear     = {:.2f}\nsubmit :: adapting1 = {:.2f}\nsubmit :: adapting2 = {:.2f}\npresent             = {:.2f}",
                nova::TimeSubmitting.exchange(0) / divisor,
                nova::TimeAdaptingFromAcquire.exchange(0)  / divisor,
                nova::TimeAdaptingToPresent.exchange(0)  / divisor,
                nova::TimePresenting.exchange(0) / divisor);

            NOVA_LOG("Atomic Operations = {}", nova::NumHandleOperations.load());

            lastTime = std::chrono::steady_clock::now();
            frames = 0;
        }

        // Pick fence and commandPool for frame in flight
        auto fif = frame % 2;
        auto fence = -fences[fif];
        auto commandPool = -commandPools[fif];
        frame++;

        // Wait for previous commands in frame to complete
        fence.Wait();

        // Acquire new images from swapchains
        queue.Acquire({swapchain1, swapchain2}, {fence});

        // Clear resource state tracking
        state.Clear(3);

        // Reset command pool and begin new command list
        commandPool.Reset();
        cmd = commandPool.Begin(state);

        // Clear screen
        cmd.Clear(swapchain1.GetCurrent(), Vec4(26 / 255.f, 89 / 255.f, 71 / 255.f, 1.f));

        // Draw ImGui demo window
        imgui.BeginFrame();
        ImGui::ShowDemoWindow();
        imgui.EndFrame(cmd, swapchain1.GetCurrent());

        // Present #1
        cmd.Present(swapchain1.GetCurrent());

        // Clear and present #2
        cmd.Clear(swapchain2.GetCurrent(), Vec4(112 / 255.f, 53 / 255.f, 132 / 255.f, 1.f));
        cmd.Present(swapchain2.GetCurrent());

        // Submit work
        queue.Submit({cmd}, {fence}, {fence});

        // Present both swapchains
        queue.Present({swapchain1, swapchain2}, {fence}, false);
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
        glfwWaitEvents();
    }

    NOVA_LOG("Allocations = {}", nova::ContextImpl::AllocationCount.load());
}