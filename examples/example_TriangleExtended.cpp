#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/rhi/nova_RHI.hpp>

#include <nova/window/nova_Window.hpp>

struct PushConstants
{
    u64 vertices;
    Vec3  offset;
};

struct Vertex
{
    Vec3 position;
    Vec3    color;
};

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

    auto swapchain = nova::Swapchain::Create(context, window.NativeHandle(),
        nova::ImageUsage::ColorAttach
        | nova::ImageUsage::TransferDst,
        nova::PresentMode::Fifo);
    NOVA_DEFER(&) { swapchain.Destroy(); };

    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);
    auto cmd_pool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);
    NOVA_DEFER(&) {
        cmd_pool.Destroy();
        fence.Destroy();
    };

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

    auto vertex_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::Vertex, "main", "", {
        // language=glsl
        R"glsl(
#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_buffer_reference2    : require
#extension GL_EXT_nonuniform_qualifier : require

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Vertex {
    vec3 position;
    vec3 color;
};

layout(push_constant, scalar) uniform pc_ {
    Vertex vertices;
    vec3     offset;
} pc;

layout(location = 0) out vec3 color;

void main() {
    Vertex v = pc.vertices[gl_VertexIndex];
    color = v.color;
    gl_Position = vec4(v.position + pc.offset, 1);
}
        )glsl"
    });
    NOVA_DEFER(&) { vertex_shader.Destroy(); };

    auto fragment_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::Fragment, "main", "", {
        // language=glsl
        R"glsl(
layout(location = 0) in vec3 in_color;
layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = vec4(in_color, 1);
}
        )glsl"
    });
    NOVA_DEFER(&) { fragment_shader.Destroy(); };

    // Draw

    NOVA_DEFER(&) { fence.Wait(); };
    while (app.ProcessEvents()) {
        fence.Wait();
        queue.Acquire({swapchain}, {fence});

        cmd_pool.Reset();
        auto cmd = cmd_pool.Begin();

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
            .vertices = vertices.DeviceAddress(),
            .offset = {0.f, 0.25f, 0.f},
        });
        cmd.BindIndexBuffer(indices, nova::IndexType::U32);
        cmd.DrawIndexed(3, 1, 0, 0, 0);
        cmd.EndRendering();

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        app.WaitForEvents();
    }
}