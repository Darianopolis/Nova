#include "example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

NOVA_EXAMPLE(TriangleMinimal, "tri-minimal")
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1200, "Nova - Triangle Minimal", nullptr, nullptr);
    NOVA_CLEANUP(&) { glfwTerminate(); };

    auto context = nova::Context::Create({ .debug = true });
    auto swapchain = nova::Swapchain::Create(context, glfwGetWin32Window(window),
        nova::TextureUsage::ColorAttach
        | nova::TextureUsage::TransferDst,
        nova::PresentMode::Fifo);
    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto cmdPool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);

    auto vertexShader = nova::Shader::Create(context, nova::ShaderStage::Vertex, {
        nova::shader::Output("color", nova::ShaderVarType::Vec3),
        nova::shader::Fragment(R"glsl(
            const vec2 positions[3] = vec2[] (vec2(-0.6, 0.6), vec2(0.6, 0.6), vec2(0, -0.6));
            const vec3    colors[3] = vec3[] (vec3(1, 0, 0),   vec3(0, 1, 0),  vec3(0, 0, 1));
            void main() {
                color = colors[gl_VertexIndex];
                gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
            }

            // let positions = [vec2(-0.6, 0.6), vec2(0.6, 0.6), vec2(0, -0.6)];
            // let color     = [vec3(1, 0, 0),   vec3(0, 1, 0),  vec3(0, 0, 1)];
            // fn main() {
            //     color = colors[gl_VertexIndex];
            //     gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
            // }
        )glsl"),
    });

    auto fragmentShader = nova::Shader::Create(context, nova::ShaderStage::Fragment, {
        nova::shader::Input("inColor", nova::ShaderVarType::Vec3),
        nova::shader::Output("fragColor", nova::ShaderVarType::Vec4),
        nova::shader::Fragment(R"glsl(
            void main() {
                fragColor = vec4(inColor, 1);
            }
        )glsl"),
    });

    while (!glfwWindowShouldClose(window)) {

        fence.Wait();
        queue.Acquire({swapchain}, {fence});
        cmdPool.Reset();
        auto cmd = cmdPool.Begin();

        cmd.BeginRendering({{}, swapchain.GetExtent()}, {swapchain.GetCurrent()});
        cmd.ClearColor(0, Vec4(Vec3(0.1f), 1.f), swapchain.GetExtent());
        cmd.ResetGraphicsState();
        cmd.SetViewports({{{}, Vec2I(swapchain.GetExtent())}}, true);
        cmd.SetBlendState({false});
        cmd.BindShaders({vertexShader, fragmentShader});
        cmd.Draw(3, 1, 0, 0);
        cmd.EndRendering();

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        glfwWaitEvents();
    }
}