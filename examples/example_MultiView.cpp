#include "main/example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>
#include <nova/window/nova_Window.hpp>

NOVA_EXAMPLE(Multiview, "multiview")
{
    nova::rhi::Init({});

    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Multiview")
        .SetSize({ 1920, 1080 }, nova::WindowPart::Client)
        .Show(true);

    auto swapchain = nova::Swapchain::Create(window,
        nova::ImageUsage::ColorAttach
        | nova::ImageUsage::TransferDst,
        nova::PresentMode::Fifo);
    NOVA_DEFER(&) { swapchain.Destroy(); };
    auto queue = nova::Queue::Get(nova::QueueFlags::Graphics, 0);

    auto image = nova::Image::Create(
        { 256, 256, 32 },
        nova::ImageUsage::ColorAttach,
        nova::Format::RGBA8_UNorm,
        nova::ImageFlags::Array);
    NOVA_DEFER(&) { image.Destroy(); };

    auto vertex_shader   = nova::Shader::Create(nova::ShaderLang::Slang, nova::ShaderStage::Vertex,   "Vertex",   "example_TriangleMinimal.slang");
    NOVA_DEFER(&) { vertex_shader.Destroy(); };
    auto fragment_shader = nova::Shader::Create(nova::ShaderLang::Slang, nova::ShaderStage::Fragment, "Fragment", "example_TriangleMinimal.slang");
    NOVA_DEFER(&) { fragment_shader.Destroy(); };

    nova::Log(NOVA_FMTEXPR(nova::rhi::Get().Properties().max_multiview_count));

    NOVA_DEFER(&) { queue.WaitIdle(); };
    while (app.ProcessEvents()) {

        queue.WaitIdle();
        queue.Acquire({swapchain}, {});
        auto cmd = queue.Begin();

        cmd.BeginRendering({
            .region = {{}, Vec2U(image.Extent())},
            .color_attachments = {image},
            .view_mask = (1u << nova::rhi::Get().Properties().max_multiview_count) - 1u,
        });
        cmd.ClearColor(0, Vec4(0.1f, 0.29f, 0.32f, 1.f), Vec2U(image.Extent()));
        cmd.ResetGraphicsState();
        cmd.SetViewports({{{}, Vec2I(image.Extent())}}, true);
        cmd.SetBlendState({false});
        cmd.BindShaders({vertex_shader, fragment_shader});
        cmd.Draw(3, 1, 0, 0);
        cmd.EndRendering();

        cmd.BlitImage(swapchain.Target(), image, nova::Filter::Nearest);

        cmd.Present(swapchain);
        queue.Submit({cmd}, {});
        queue.Present({swapchain}, {});

        app.WaitForEvents();
    }
}