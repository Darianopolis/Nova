#include "main/example_Main.hpp"

#include <nova/core/nova_Timer.hpp>

#include <nova/rhi/nova_RHI.hpp>

#include <nova/window/nova_Window.hpp>

NOVA_EXAMPLE(CommandTest, "cmd-test")
{
    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Triangle Minimal")
        .SetSize({ 1920, 1080 }, nova::WindowPart::Client)
        .Show(true);

    auto context = nova::Context::Create({
        .debug = false,
    });
    NOVA_DEFER(&) { context.Destroy(); };
    auto swapchain = nova::Swapchain::Create(context, window.NativeHandle(),
        nova::ImageUsage::ColorAttach
        | nova::ImageUsage::TransferDst,
        nova::PresentMode::Immediate);
    NOVA_DEFER(&) { swapchain.Destroy(); };
    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);

    std::array<nova::SyncPoint, 2> wait_values;
    // std::array<nova::CommandPool, 2> pools { nova::CommandPool::Create(context, queue), nova::CommandPool::Create(context, queue) };
    u32 fif = 0;
    auto last_time = std::chrono::steady_clock::now();
    auto frames = 0;

    std::array<std::vector<nova::CommandList>, 2> lists;
    lists[0].resize(1'000);
    lists[1].resize(1'000);

    int buffer_count = 100, command_count = 1000;
    // int buffer_count = 10, command_count = 10'000;
    // int buffer_count = 1, command_count = 100'000;

    NOVA_DEFER(&) { queue.WaitIdle(); };
    while (app.ProcessEvents()) {

        // Debug output statistics
        frames++;
        auto new_time = std::chrono::steady_clock::now();
        if (new_time - last_time > 1s) {
            nova::Log("\nFrame time = {} ({} fps)\nAllocations = {:3} (+ {} /s)",
                nova::DurationToString((new_time - last_time) / frames), frames, nova::rhi::stats::AllocationCount.load(),
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

        fif = 1 - fif;
        wait_values[fif].Wait();
        queue.Acquire({swapchain});

        // auto& pool = pools[fif];
        // pool.Reset();
        auto& pool = queue;

        auto& list = lists[fif];
        auto& last_list = lists[1 - fif];

        if (last_list[0]) {
            for (int i = 0; i < buffer_count; i++) {
                last_list[i].Discard();
            }
        }

        for (int i = 0; i < buffer_count; i++) {
            list[i] = pool.Begin();
            for (int j = 0; j < command_count; ++j) {
                list[i].SetCullState(nova::CullMode::Back, nova::FrontFace::CounterClockwise);
            }
            list[i].End();
        }

        auto cmd = pool.Begin();

        cmd.Present(swapchain);
        wait_values[fif] = queue.Submit({cmd}, {});
        queue.Present({swapchain}, {});
    }
}