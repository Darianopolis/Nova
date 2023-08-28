#include "example_Main.hpp"

#include <nova/core/nova_Timer.hpp>
#include <nova/rhi/nova_RHI.hpp>
#include <nova/ui/nova_ImGui.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

NOVA_EXAMPLE(multi)
{
    auto context = nova::Context::Create({
        .debug = false,
    });
    NOVA_CLEANUP(&) { context.Destroy(); };

    auto presentMode = nova::PresentMode::Mailbox;
    auto swapchainUsage = nova::TextureUsage::ColorAttach | nova::TextureUsage::Storage;

    glfwInit();
    NOVA_CLEANUP(&) { glfwTerminate(); };
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* windows[] {
        glfwCreateWindow(1920, 1200, "Nova - Multi Target #1", nullptr, nullptr),
        glfwCreateWindow(1920, 1200, "Nova - Multi Target #2", nullptr, nullptr),
    };
    NOVA_CLEANUP(&) {
        glfwDestroyWindow(windows[0]);
        glfwDestroyWindow(windows[1]);
    };
    nova::Swapchain swapchains[] {
        nova::Swapchain::Create(context, glfwGetWin32Window(windows[0]), swapchainUsage, presentMode),
        nova::Swapchain::Create(context, glfwGetWin32Window(windows[1]), swapchainUsage, presentMode),
    };
    NOVA_CLEANUP(&) {
        swapchains[0].Destroy();
        swapchains[1].Destroy();
    };

    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto state = nova::CommandState::Create(context);
    u64 waitValues[] { 0ull, 0ull };
    auto fence = nova::Fence::Create(context);
    nova::CommandPool commandPools[] {
        nova::CommandPool::Create(context, queue),
        nova::CommandPool::Create(context, queue)
    };
    auto heap = nova::DescriptorHeap::Create(context, 2);
    auto sampler = nova::Sampler::Create(context, nova::Filter::Linear, nova::AddressMode::Repeat, nova::BorderColor::TransparentBlack, 0.f);
    NOVA_CLEANUP(&) {
        state.Destroy();
        fence.Destroy();
        commandPools[0].Destroy();
        commandPools[1].Destroy();
        heap.Destroy();
        sampler.Destroy();
    };

    heap.WriteSampler(0, sampler);

    auto imgui = nova::ImGuiLayer({
        .window = windows[0],
        .context = context,
        .heap = heap,
        .sampler = 0,
        .fontTextureID = 1,
    }, state);

    u64 frame = 0;
    auto lastTime = std::chrono::steady_clock::now();
    auto frames = 0;
    auto update = [&] {

        // Debug output statistics
        frames++;
        auto newTime = std::chrono::steady_clock::now();
        if (newTime - lastTime > 1s) {
            NOVA_LOG("\nFps = {}\nAllocations = {:3} (+ {} /s)",
                frames, nova::rhi::stats::AllocationCount.load(),
                nova::rhi::stats::NewAllocationCount.exchange(0));
            f64 divisor = 1000.0 * frames;
            NOVA_LOG("submit :: clear     = {:.2f}\n"
                     "submit :: adapting1 = {:.2f}\n"
                     "submit :: adapting2 = {:.2f}\n"
                     "present             = {:.2f}",
                nova::rhi::stats::TimeSubmitting.exchange(0) / divisor,
                nova::rhi::stats::TimeAdaptingFromAcquire.exchange(0)  / divisor,
                nova::rhi::stats::TimeAdaptingToPresent.exchange(0)  / divisor,
                nova::rhi::stats::TimePresenting.exchange(0) / divisor);

            lastTime = std::chrono::steady_clock::now();
            frames = 0;
        }

        // Pick fence and commandPool for frame in flight
        auto fif = frame++ % 2;

        // Wait for previous commands in frame to complete
        fence.Wait(waitValues[fif]);

        // Acquire new images from swapchains
        queue.Acquire({swapchains[0], swapchains[1]}, {fence});

        // Clear resource state tracking
        // state.Clear(3);

        // Reset command pool and begin new command list
        commandPools[fif].Reset();
        auto cmd = commandPools[fif].Begin(state);

        // Clear screen
        cmd.Clear(swapchains[0].GetCurrent(), Vec4(26 / 255.f, 89 / 255.f, 71 / 255.f, 1.f));

        // Draw ImGui demo window
        imgui.BeginFrame();
        ImGui::ShowDemoWindow();
        imgui.DrawFrame(cmd, swapchains[0].GetCurrent(), fence);

        // Present #1
        cmd.Present(swapchains[0]);

        // Clear and present #2
        cmd.Clear(swapchains[1].GetCurrent(), Vec4(112 / 255.f, 53 / 255.f, 132 / 255.f, 1.f));
        cmd.Present(swapchains[1]);

        // Submit work
        queue.Submit({cmd}, {fence}, {fence});

        // Present both swapchains
        queue.Present({swapchains[0], swapchains[1]}, {fence}, false);

        waitValues[fif] = fence.GetPendingValue();
    };

    NOVA_CLEANUP(&) { fence.Wait(); };

    for (auto window : windows) {
        glfwSetWindowUserPointer(window, &update);
        glfwSetWindowSizeCallback(window, [](auto w, int,int) {
            (*static_cast<decltype(update)*>(glfwGetWindowUserPointer(w)))();
        });
    }

    while (!glfwWindowShouldClose(windows[0]) && !glfwWindowShouldClose(windows[1])) {
        update();
        glfwPollEvents();
    }
}