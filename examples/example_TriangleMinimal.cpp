#include "main/example_Main.hpp"

#include <nova/core/nova_ToString.hpp>
#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Timer.hpp>
#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

#include <nova/window/nova_Window.hpp>

NOVA_EXAMPLE(TriangleMinimal, "tri-min")
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
        nova::PresentMode::Mailbox);
    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);

    auto vertex_shader = nova::Shader::Create(context,nova::ShaderLang::Glsl, nova::ShaderStage::Vertex, "main", "", {
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
        R"glsl(
layout(location = 0) in vec3 in_color;
layout(location = 0) out vec4 frag_color;
void main() {
    frag_color = vec4(in_color, 1.0);
}
        )glsl"
    });

    constexpr u32 FifCount = 2;

    std::array<nova::SyncPoint, FifCount> wait_values;
    u32 fif = 0;
    auto last_time = std::chrono::steady_clock::now();
    auto frames = 0;

    while (app.ProcessEvents()) {

        // Debug output statistics
        frames++;
        auto new_time = std::chrono::steady_clock::now();
        if (new_time - last_time > 1s) {
            NOVA_LOG("\nFrame time = {} ({} fps)\nAllocations = {:3} (+ {} /s)",
                nova::DurationToString((new_time - last_time) / frames), frames, nova::rhi::stats::AllocationCount.load(),
                nova::rhi::stats::NewAllocationCount.exchange(0));
            f64 divisor = 1000.0 * frames;
            NOVA_LOG("submit :: clear     = {:.2f}\n"
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

        fif = (fif + 1) % FifCount;
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