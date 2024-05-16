#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Timer.hpp>
#include <nova/rhi/nova_RHI.hpp>
#include <nova/ui/nova_ImGui.hpp>
#include <nova/core/nova_ToString.hpp>

#include <imgui.h>

NOVA_EXAMPLE(MultiPresent, "multi-present")
{
    auto context = nova::Context::Create({
        .debug = false,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    auto present_mode = nova::PresentMode::Mailbox;
    auto swapchain_usage = nova::ImageUsage::ColorAttach | nova::ImageUsage::Storage;

    auto app = nova::Application::Create();

    constexpr u32 NumWindows = 4;

    std::vector<nova::Window> windows;
    std::vector<nova::Swapchain> swapchains;

    for (u32 i = 0; i < NumWindows; ++i) {
        auto window = windows.emplace_back(nova::Window::Create(app).SetTitle(std::format("Nova - Multi Present {}", i)).SetSize({ 1920, 1080 }, nova::WindowPart::Client).Show(true));
        swapchains.emplace_back(nova::Swapchain::Create(context, window.NativeHandle(), swapchain_usage, present_mode));
    }
    NOVA_DEFER(&) {
        for (auto swapchain : swapchains) {
            swapchain.Destroy();
        }
    };

    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);
    std::array<nova::SyncPoint, 2> wait_values;
    auto sampler = nova::Sampler::Create(context, nova::Filter::Linear,
        nova::AddressMode::Repeat, nova::BorderColor::TransparentBlack, 0.f);
    NOVA_DEFER(&) { sampler.Destroy(); };

    u64 frame = 0;
    auto last_time = std::chrono::steady_clock::now();
    auto frames = 0;
    auto update = [&] {
        // Debug output statistics
        frames++;
        auto new_time = std::chrono::steady_clock::now();
        if (new_time - last_time > 1s) {
            NOVA_LOG("\nFrametime = {} ({} fps)\nAllocations = {:3} (+ {} /s)",
                nova::DurationToString((new_time - last_time) / frames), frames, nova::rhi::stats::AllocationCount.load(),
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
        wait_values[fif].Wait();

        NOVA_STACK_POINT();
        auto hswapchains = NOVA_STACK_ALLOC(nova::HSwapchain, swapchains.size());
        for (u32 i = 0; i < swapchains.size(); ++i) {
            hswapchains[i] = swapchains[i];
        }
        auto swapchain_span = nova::Span<nova::HSwapchain>(hswapchains, swapchains.size());

        // Acquire new images from swapchains
        queue.Acquire(swapchain_span);

        // Reset command pool and begin new command list
        auto cmd = queue.Begin();

        for (auto& swapchain : swapchains) {
           auto target = swapchain.Target();

            // Clear screen
            cmd.ClearColor(target, Vec4(26 / 255.f, 89 / 255.f, 71 / 255.f, 1.f));

            // Present #1
            cmd.Present(swapchain);
        }

        // Submit work
        wait_values[fif] = queue.Submit({cmd}, {});

        // Present both swapchains
        {
            queue.Present(swapchain_span, {});
        }
    };

    NOVA_DEFER(&) { queue.WaitIdle(); };

    while (app.ProcessEvents()) {
        update();
    }
}