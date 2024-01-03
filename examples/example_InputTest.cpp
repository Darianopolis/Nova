#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/rhi/nova_RHI.hpp>

#include <nova/ui/nova_ImGui.hpp>

#include <nova/window/nova_Window.hpp>

NOVA_EXAMPLE(InputTest, "input-test")
{
    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Triangle Minimal")
        .SetSize({ 1920, 1080 }, nova::WindowPart::Client)
        .Show(true);

    auto context = nova::Context::Create({
        .debug = true,
    });
    auto swapchain = nova::Swapchain::Create(context, window.NativeHandle(),
        nova::ImageUsage::ColorAttach
        | nova::ImageUsage::TransferDst,
        nova::PresentMode::Fifo);
    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);
    auto cmd_pool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);
    auto sampler = nova::Sampler::Create(context, nova::Filter::Linear,
        nova::AddressMode::Repeat, nova::BorderColor::TransparentBlack, 0.f);

    NOVA_DEFER(&)
    {
        sampler.Destroy();
        fence.Destroy();
        cmd_pool.Destroy();
        swapchain.Destroy();
        context.Destroy();
    };

    auto imgui = nova::imgui::ImGuiLayer({
        .window = window,
        .context = context,
        .sampler = sampler,
    });

    NOVA_DEFER(&)
    {
        fence.Wait();
    };

    while (app.ProcessEvents()) {

        fence.Wait();
        queue.Acquire(swapchain, {fence});
        cmd_pool.Reset();
        auto cmd = cmd_pool.Begin();

        cmd.ClearColor(swapchain.Target(), Vec4(0.f));

        imgui.BeginFrame();
        app.DebugInputState();
        imgui.DrawFrame(cmd, swapchain.Target(), fence);

        cmd.Present(swapchain);
        queue.Submit(cmd, fence, fence);
        queue.Present(swapchain, fence);

        app.WaitForEvents();
    }
}