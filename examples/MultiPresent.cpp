#include "main/Main.hpp"

#include <nova/gpu/RHI.hpp>
#include <nova/gui/ImGui.hpp>

#include <nova/gpu/vulkan/VulkanRHI.hpp>

NOVA_EXAMPLE(MultiPresent, "multi-present")
{
    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };

    auto context = nova::Context::Create({
        .debug = true,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    auto present_mode = nova::PresentMode::Mailbox;
    auto swapchain_usage = nova::ImageUsage::ColorAttach | nova::ImageUsage::Storage;

    constexpr u32 NumWindows = 4;

    std::vector<nova::Window> windows;
    std::vector<nova::Swapchain> swapchains;

    NOVA_DEFER(&) {
        for (auto swapchain : swapchains) {
            swapchain.Destroy();
        }
    };

    for (u32 i = 0; i < NumWindows; ++i) {
        auto window = windows.emplace_back(nova::Window::Create(app)
            .SetTitle(std::format("Nova - Multi Present {}", i))
            .SetSize({ 1920, 1080 }, nova::WindowPart::Client)
            .Show(true));
        swapchains.emplace_back(nova::Swapchain::Create(context, window, swapchain_usage, present_mode));
    }

    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);
    std::array<nova::SyncPoint, 2> wait_values;
    auto sampler = nova::Sampler::Create(context, nova::Filter::Linear,
        nova::AddressMode::Repeat, nova::BorderColor::TransparentBlack, 0.f);
    NOVA_DEFER(&) { sampler.Destroy(); };

    app.AddCallback([&](const nova::AppEvent& e) {
        if (e.type == nova::EventType::WindowClosing) {
            auto i = std::distance(windows.begin(), std::ranges::find(windows, e.window));
            queue.WaitIdle();
            swapchains[i].Destroy();
        }
    });

    u64 frame = 0;
    auto last_time = std::chrono::steady_clock::now();
    auto frames = 0;
    auto update = [&] {
        // Debug output statistics
        frames++;
        auto new_time = std::chrono::steady_clock::now();
        if (new_time - last_time > 1s) {
            nova::Log("Frametime   = {} ({} fps)"
                    "\nAllocations = {:3} (+ {} /s)",
                nova::DurationToString((new_time - last_time) / frames), frames, nova::rhi::stats::AllocationCount.load(),
                nova::rhi::stats::NewAllocationCount.exchange(0));
            f64 divisor = 1000.0 * frames;
            nova::Log("submit :: clear     = {:.2f}"
                    "\nsubmit :: adapting1 = {:.2f}"
                    "\nsubmit :: adapting2 = {:.2f}"
                    "\npresent             = {:.2f}\n",
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
        usz swapchain_count = 0;
        auto hswapchains = NOVA_STACK_ALLOC(nova::HSwapchain, swapchains.size());
        for (u32 i = 0; i < swapchains.size(); ++i) {
            if (swapchains[i]) hswapchains[swapchain_count++] = swapchains[i];
        }
        auto swapchain_span = nova::Span<nova::HSwapchain>(hswapchains, swapchain_count);

        // Acquire new images from swapchains
        queue.Acquire(swapchain_span);

        // Reset command pool and begin new command list
        auto cmd = queue.Begin();

        for (auto& swapchain : swapchain_span) {
           auto target = swapchain.Unwrap().Target();

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
