#include "example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

NOVA_EXAMPLE(TriangleMeshShader, "tri-meshshader")
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1200, "Nova - Triangle Mesh Shader", nullptr, nullptr);
    NOVA_CLEANUP(&) { glfwTerminate(); };

    auto context = nova::Context::Create({
        .debug = true,
    });
    auto swapchain = nova::Swapchain::Create(context, glfwGetWin32Window(window),
        nova::TextureUsage::ColorAttach
        | nova::TextureUsage::TransferDst,
        nova::PresentMode::Fifo);
    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto cmdPool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);

    auto taskShader = nova::Shader::Create(context, nova::ShaderStage::Task, {
        nova::shader::Kernel(R"glsl(
            EmitMeshTasksEXT(3, 1, 1);
        )glsl")
    });

    auto meshShader = nova::Shader::Create(context, nova::ShaderStage::Mesh, {
        nova::shader::Fragment(R"glsl(
            layout(triangles, max_vertices = 3, max_primitives = 1) out;
            layout(location = 0) out vec3 outColors[];

            const vec2 positions[3] = vec2[] (vec2(-0.6, 0.6), vec2(0.6, 0.6), vec2(0, -0.6));
            const vec3    colors[3] = vec3[] (vec3(1, 0, 0),   vec3(0, 1, 0),  vec3(0, 0, 1));
        )glsl"),
        nova::shader::ComputeKernel(Vec3U(1), R"glsl(
            uint gid = gl_GlobalInvocationID.x;

            SetMeshOutputsEXT(3, 1);
            for (int i = 0; i < 3; ++i) {
                gl_MeshVerticesEXT[i].gl_Position = vec4(
                    positions[i] + vec2((gid - 1.f) / 3.f), gid / 3.f, 1);
                outColors[i] = colors[i];
            }
	        gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);
        )glsl"),
    });

    auto fragmentShader = nova::Shader::Create(context, nova::ShaderStage::Fragment, {
        nova::shader::Fragment(R"glsl(
            layout(location = 0) in vec3 inColor;
            layout(location = 0) out vec4 fragColor;
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
        cmd.BindShaders({taskShader, meshShader, fragmentShader});
        cmd.DrawMeshTasks(Vec3U(1));
        cmd.EndRendering();

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        glfwWaitEvents();
    }
}