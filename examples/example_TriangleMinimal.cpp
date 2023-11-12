#include "example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/vulkan/glsl/nova_VulkanGlsl.hpp>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

NOVA_EXAMPLE(TriangleMinimal, "tri-min")
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1200, "Nova - Triangle Minimal", nullptr, nullptr);
    NOVA_DEFER(&) { glfwTerminate(); };

    auto context = nova::Context::Create({
        .debug = true
    });
    auto swapchain = nova::Swapchain::Create(context, glfwGetWin32Window(window),
        nova::TextureUsage::ColorAttach
        | nova::TextureUsage::TransferDst,
        nova::PresentMode::Fifo);
    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto cmd_pool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);

    auto vertex_shader = nova::Shader::Create(context, nova::ShaderStage::Vertex, "main",
        nova::glsl::Compile(nova::ShaderStage::Vertex, "main", "", {R"glsl(
            layout(location = 0) out vec3 color;
            const vec2 positions[3] = vec2[] (vec2(-0.6, 0.6), vec2(0.6, 0.6), vec2(0, -0.6));
            const vec3    colors[3] = vec3[] (vec3(1, 0, 0),   vec3(0, 1, 0),  vec3(0, 0, 1));
            void main() {
                color = colors[gl_VertexIndex];
                gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
            }
        )glsl"}));

    auto fragment_shader = nova::Shader::Create(context, nova::ShaderStage::Fragment, "main",
        nova::glsl::Compile(nova::ShaderStage::Fragment, "main", "", {R"glsl(
            layout(location = 0) in vec3 in_color;
            layout(location = 0) out vec4 frag_color;
            void main() {
                frag_color = vec4(in_color, 1);
            }
        )glsl"}));

    while (!glfwWindowShouldClose(window)) {

        fence.Wait();
        queue.Acquire({swapchain}, {fence});
        cmd_pool.Reset();
        auto cmd = cmd_pool.Begin();

        cmd.BeginRendering({{}, swapchain.GetExtent()}, {swapchain.GetCurrent()});
        cmd.ClearColor(0, Vec4(Vec3(0.1f), 1.f), swapchain.GetExtent());
        cmd.ResetGraphicsState();
        cmd.SetViewports({{{}, Vec2I(swapchain.GetExtent())}}, true);
        cmd.SetBlendState({false});
        cmd.BindShaders({vertex_shader, fragment_shader});
        cmd.Draw(3, 1, 0, 0);
        cmd.EndRendering();

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        glfwWaitEvents();
    }
}