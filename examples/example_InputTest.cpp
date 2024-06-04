#include "main/example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>
#include <nova/ui/nova_ImGui.hpp>
#include <nova/window/nova_Window.hpp>

NOVA_EXAMPLE(InputTest, "input-test")
{
    nova::rhi::Init({});

    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Triangle Minimal")
        .SetSize({ 1920, 1080 }, nova::WindowPart::Client)
        .Show(true);

    auto swapchain = nova::Swapchain::Create(window,
        nova::ImageUsage::ColorAttach
        | nova::ImageUsage::TransferDst,
        nova::PresentMode::Fifo);
    auto queue = nova::Queue::Get(nova::QueueFlags::Graphics, 0);
    auto sampler = nova::Sampler::Create(nova::Filter::Linear,
        nova::AddressMode::Repeat, nova::BorderColor::TransparentBlack, 0.f);

    NOVA_DEFER(&) {
        sampler.Destroy();
        swapchain.Destroy();
    };

    auto imgui = nova::imgui::ImGuiLayer({
        .window = window,
        .sampler = sampler,
        .frames_in_flight = 1,
    });

    NOVA_DEFER(&) { queue.WaitIdle(); };

    while (app.ProcessEvents()) {

        queue.WaitIdle();
        queue.Acquire(swapchain);
        auto cmd = queue.Begin();

        cmd.ClearColor(swapchain.Target(), Vec4(0.f));

        imgui.BeginFrame();
        app.DebugInputState();
        imgui.DrawFrame(cmd, swapchain.Target());

        cmd.Present(swapchain);
        queue.Submit(cmd, {});
        queue.Present(swapchain, {});

        app.WaitForEvents();
    }
}