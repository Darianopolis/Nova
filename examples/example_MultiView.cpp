#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/rhi/nova_RHI.hpp>

#include <nova/window/nova_Window.hpp>

NOVA_EXAMPLE(Multiview, "multiview")
{
    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Multiview")
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

    auto image = nova::Image::Create(context,
        { 256, 256, 32 },
        nova::ImageUsage::ColorAttach,
        nova::Format::RGBA8_UNorm,
        nova::ImageFlags::Array);

    auto vertex_shader = nova::Shader::Create(context,nova::ShaderLang::Glsl, nova::ShaderStage::Vertex, "main", "", {
        // language=glsl
        R"glsl(
layout(location = 0) out vec3 color;
const vec2 positions[3] = vec2[] (vec2(-0.6, 0.6), vec2(0.6, 0.6), vec2(0, -0.6));
const vec3    colors[3] = vec3[] (vec3(1, 0, 0),   vec3(0, 1, 0),  vec3(0, 0, 1));
void main() {
    color = colors[gl_VertexIndex];
    gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
}
        )glsl"
    });

    auto fragment_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::Fragment, "main", "", {
        // language=glsl
        R"glsl(
layout(location = 0) in vec3 in_color;
layout(location = 0) out vec4 frag_color;
void main() {
    frag_color = vec4(in_color, 0.1);
}
        )glsl"
    });

    NOVA_LOGEXPR(context.Properties().max_multiview_count);

    while (app.ProcessEvents()) {

        fence.Wait();
        queue.Acquire({swapchain}, {fence});
        cmd_pool.Reset();
        auto cmd = cmd_pool.Begin();

        // cmd.BeginRendering({{}, Vec2U(image.GetExtent())}, {image});
        cmd.BeginRendering({
            .region = {{}, Vec2U(image.Extent())},
            .color_attachments = {image},
            .view_mask = (1u << context.Properties().max_multiview_count) - 1u,
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
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        app.WaitForEvents();
    }
}