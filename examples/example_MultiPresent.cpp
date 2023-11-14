#include "example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Timer.hpp>
#include <nova/rhi/nova_RHI.hpp>
#include <nova/ui/nova_ImGui.hpp>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>


NOVA_EXAMPLE(MultiPresent, "multi-present")
{
    auto context = nova::Context::Create({
        .debug = true,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    auto present_mode = nova::PresentMode::Mailbox;
    auto swapchain_usage = nova::ImageUsage::ColorAttach | nova::ImageUsage::Storage;

    glfwInit();
    NOVA_DEFER(&) { glfwTerminate(); };

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto windows = std::array {
        glfwCreateWindow(1920, 1200, "Nova - Multi Present #1", nullptr, nullptr),
        glfwCreateWindow(1920, 1200, "Nova - Multi Present #2", nullptr, nullptr),
    };

    auto swapchains = std::array {
        nova::Swapchain::Create(context, glfwGetWin32Window(windows[0]), swapchain_usage, present_mode),
        nova::Swapchain::Create(context, glfwGetWin32Window(windows[1]), swapchain_usage, present_mode),
    };
    NOVA_DEFER(&) {
        swapchains[0].Destroy();
        swapchains[1].Destroy();
    };

    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto wait_values = std::array { 0ull, 0ull };
    auto fence = nova::Fence::Create(context);
    auto command_pools = std::array {
        nova::CommandPool::Create(context, queue),
        nova::CommandPool::Create(context, queue)
    };
    auto sampler = nova::Sampler::Create(context, nova::Filter::Linear,
        nova::AddressMode::Repeat, nova::BorderColor::TransparentBlack, 0.f);
    NOVA_DEFER(&) {
        fence.Destroy();
        command_pools[0].Destroy();
        command_pools[1].Destroy();
        sampler.Destroy();
    };

    auto imgui = nova::imgui::ImGuiLayer({
        .window = windows[0],
        .context = context,
        .sampler = sampler,
    });

    u64 frame = 0;
    auto last_time = std::chrono::steady_clock::now();
    auto frames = 0;
    auto update = [&] {

        // Debug output statistics
        frames++;
        auto new_time = std::chrono::steady_clock::now();
        if (new_time - last_time > 1s) {
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

            last_time = std::chrono::steady_clock::now();
            frames = 0;
        }

        // Pick fence and command pool for frame in flight
        auto fif = frame++ % 2;

        // Wait for previous commands in frame to complete
        fence.Wait(wait_values[fif]);

        // Acquire new images from swapchains
        queue.Acquire({swapchains[0], swapchains[1]}, {fence});

        // Reset command pool and begin new command list
        command_pools[fif].Reset();
        auto cmd = command_pools[fif].Begin();

        // Clear screen
        cmd.ClearColor(swapchains[0].GetCurrent(), Vec4(26 / 255.f, 89 / 255.f, 71 / 255.f, 1.f));

        // Draw ImGui demo window
        imgui.BeginFrame();
        ImGui::ShowDemoWindow();
        imgui.DrawFrame(cmd, swapchains[0].GetCurrent(), fence);

        // Present #1
        cmd.Present(swapchains[0]);

        // Clear and present #2
        cmd.ClearColor(swapchains[1].GetCurrent(), Vec4(112 / 255.f, 53 / 255.f, 132 / 255.f, 1.f));
        cmd.Present(swapchains[1]);

        // Submit work
        queue.Submit({cmd}, {fence}, {fence});

        // Present both swapchains
        queue.Present({swapchains[0], swapchains[1]}, {fence}, false);

        wait_values[fif] = fence.GetPendingValue();
    };

    NOVA_DEFER(&) { fence.Wait(); };

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