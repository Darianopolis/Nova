#include "main/Main.hpp"

#include <nova/gpu/RHI.hpp>
#include <nova/gui/ImGui.hpp>

NOVA_EXAMPLE(ImGuiTest, "imgui")
{
    auto context = nova::Context::Create({
        .debug = true,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    auto app = nova::Application::Create();

    nova::Window window = nova::Window::Create(app)
        .SetTitle("Nova - ImGui")
        .SetSize({ 1920, 1080 }, nova::WindowPart::Client)
        .Show(true);

    auto swapchain = nova::Swapchain::Create(context, window,
        nova::ImageUsage::ColorAttach,
        nova::PresentMode::Mailbox);
    NOVA_DEFER(&) { swapchain.Destroy(); };

    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);
    std::array<nova::SyncPoint, 2> wait_values;
    auto sampler = nova::Sampler::Create(context, nova::Filter::Linear,
        nova::AddressMode::Repeat, nova::BorderColor::TransparentBlack, 0.f);
    NOVA_DEFER(&) { sampler.Destroy(); };

    auto imgui = nova::imgui::ImGuiLayer({
        .window = window,
        .context = context,
        .sampler = sampler,
        .frames_in_flight = 2,
    });

    u64 frame = 0;
    auto last_time = std::chrono::steady_clock::now();
    auto frames = 0;
    auto update = [&] {

        // Debug output statistics
        frames++;
        auto new_time = std::chrono::steady_clock::now();
        if (new_time - last_time > 1s) {
            nova::Log("\nFps = {}\nAllocations = {:3} (+ {} /s)",
                frames, nova::rhi::stats::AllocationCount.load(),
                nova::rhi::stats::NewAllocationCount.exchange(0));
            f64 divisor = 1000.0 * frames;
            nova::Log("submit :: clear     = {:.2f}\n"
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

        // Acquire new images from swapchains
        queue.Acquire({swapchain});

        // Reset command pool and begin new command list
        auto cmd = queue.Begin();

        // Clear screen
        cmd.ClearColor(swapchain.Target(), Vec4(26 / 255.f, 89 / 255.f, 71 / 255.f, 1.f));

        // Draw ImGui demo window
        imgui.BeginFrame();
        ImGui::ShowDemoWindow();
        imgui.DrawFrame(cmd, swapchain.Target());

        // Present #1
        cmd.Present(swapchain);

        // Submit work
        wait_values[fif] = queue.Submit({cmd}, {});

        // Present both swapchains
        queue.Present({swapchain}, {});

        if (frames > 100) {
            nova::rhi::stats::ThrowOnAllocation = true;
        }
    };

    NOVA_DEFER(&) { queue.WaitIdle(); };

    while (app.ProcessEvents()) {
        update();
    }
}
