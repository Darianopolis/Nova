#include "main/example_Main.hpp"
#include "example_TriangleExtended.slang"

#include <nova/rhi/nova_RHI.hpp>
#include <nova/window/nova_Window.hpp>
#include <nova/vfs/nova_VirtualFilesystem.hpp>

NOVA_EXAMPLE(TriangleBuffered, "tri-ext")
{
    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Triangle Extended")
        .SetSize({ 1920, 1080 }, nova::WindowPart::Client)
        .Show(true);

    auto context = nova::Context::Create({
        .debug = true,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    auto swapchain = nova::Swapchain::Create(context, window,
        nova::ImageUsage::ColorAttach
        | nova::ImageUsage::TransferDst,
        nova::PresentMode::Fifo);
    NOVA_DEFER(&) { swapchain.Destroy(); };

    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);

    // Vertex data

    auto vertices = nova::Buffer::Create(context, 3 * sizeof(Vertex),
        nova::BufferUsage::Storage,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_DEFER(&) { vertices.Destroy(); };
    vertices.Set<Vertex>({
        {{ -0.6f, 0.6f, 0.f }, { 1.f, 0.f, 0.f }},
        {{  0.6f, 0.6f, 0.f }, { 0.f, 1.f, 0.f }},
        {{  0.f, -0.6f, 0.f }, { 0.f, 0.f, 1.f }}
    });

    // Indices

    auto indices = nova::Buffer::Create(context, 6 * 4,
        nova::BufferUsage::Index,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_DEFER(&) { indices.Destroy(); };
    indices.Set<u32>({0, 1, 2});

    // Shaders

    auto vertex_shader = nova::Shader::Create(context, nova::ShaderLang::Slang, nova::ShaderStage::Vertex, "VertexShader", "example_TriangleExtended.slang");
    NOVA_DEFER(&) { vertex_shader.Destroy(); };

    auto fragment_shader = nova::Shader::Create(context, nova::ShaderLang::Slang, nova::ShaderStage::Fragment, "FragmentShader", "example_TriangleExtended.slang");
    NOVA_DEFER(&) { fragment_shader.Destroy(); };

    // Draw

    NOVA_DEFER(&) { queue.WaitIdle(); };
    while (app.ProcessEvents()) {
        queue.WaitIdle();
        queue.Acquire({swapchain});

        auto cmd = queue.Begin();

        cmd.BeginRendering({
            .region = {{}, swapchain.Extent()},
            .color_attachments = {swapchain.Target()}
        });
        cmd.ClearColor(0, Vec4(Vec3(0.1f), 1.f), swapchain.Extent());

        cmd.ResetGraphicsState();
        cmd.SetViewports({{{}, Vec2I(swapchain.Extent())}}, true);
        cmd.SetBlendState({false});
        cmd.BindShaders({vertex_shader, fragment_shader});

        cmd.PushConstants(PushConstants {
            .vertices = (Vertex*)vertices.DeviceAddress(),
            .offset = {0.f, 0.25f, 0.f},
        });
        cmd.BindIndexBuffer(indices, nova::IndexType::U32);
        cmd.DrawIndexed(3, 1, 0, 0, 0);
        cmd.EndRendering();

        cmd.Present(swapchain);
        queue.Submit({cmd}, {});
        queue.Present({swapchain}, {});

        app.WaitForEvents();
    }
}