#include "main/example_Main.hpp"

#include <nova/window/nova_Window.hpp>
#include <nova/rhi/nova_RHI.hpp>

NOVA_EXAMPLE(TriangleMinimal, "tri-min")
{
    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Triangle Minimal")
        .SetSize({ 1920, 1080 }, nova::WindowPart::Client)
        .Show(true);

    auto context = nova::Context::Create({ .debug = true });
    NOVA_DEFER(&) { context.Destroy(); };
    auto swapchain = nova::Swapchain::Create(context, window.NativeHandle(),
        nova::ImageUsage::ColorAttach
        | nova::ImageUsage::TransferDst,
        nova::PresentMode::Mailbox);
    NOVA_DEFER(&) { swapchain.Destroy(); };
    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);

    auto vertex_shader   = nova::Shader::Create(context, nova::ShaderLang::Slang, nova::ShaderStage::Vertex,   "Vertex",   "example_TriangleMinimal.slang");
    NOVA_DEFER(&) { vertex_shader.Destroy(); };
    auto fragment_shader = nova::Shader::Create(context, nova::ShaderLang::Slang, nova::ShaderStage::Fragment, "Fragment", "example_TriangleMinimal.slang");
    NOVA_DEFER(&) { fragment_shader.Destroy(); };

    std::array<nova::SyncPoint, 2> wait_values;
    u32 fif = 0;

    NOVA_DEFER(&) { queue.WaitIdle(); };
    while (app.ProcessEvents()) {

        fif = (fif + 1) % wait_values.size();
        wait_values[fif].Wait();
        queue.Acquire({swapchain});
        auto cmd = queue.Begin();

        cmd.ResetGraphicsState();
        cmd.SetViewports({{{}, Vec2I(swapchain.Extent())}}, true);
        cmd.SetBlendState({true});
        cmd.BindShaders({vertex_shader, fragment_shader});

        cmd.BeginRendering({
            .region = {{}, swapchain.Extent()},
            .color_attachments = {swapchain.Target()}
        });
        cmd.ClearColor(0, Vec4(0.1f, 0.29f, 0.32f, 1.f), swapchain.Extent());
        cmd.Draw(3, 1, 0, 0);
        cmd.EndRendering();

        cmd.Present(swapchain);
        wait_values[fif] = queue.Submit({cmd}, {});
        queue.Present({swapchain}, {});
    }
}