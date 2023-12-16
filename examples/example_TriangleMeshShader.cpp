#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/rhi/nova_RHI.hpp>

#include <nova/window/nova_Window.hpp>

NOVA_EXAMPLE(TriangleMeshShader, "tri-mesh")
{
    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app, {
        .title = "Nova - Triangle Mesh Shader",
        .size = { 1920, 1080 },
    });

    auto context = nova::Context::Create({
        .debug = true,
    });
    auto swapchain = nova::Swapchain::Create(context, window.GetNativeHandle(),
        nova::ImageUsage::ColorAttach
        | nova::ImageUsage::TransferDst,
        nova::PresentMode::Fifo);
    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto cmd_pool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);

    auto task_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::Task, "main", "", {
        // language=glsl
        R"glsl(
            #extension GL_EXT_mesh_shader : require

            void main()
            {
                EmitMeshTasksEXT(1, 1, 1);
            }
        )glsl"
    });

    auto mesh_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::Mesh, "main", "", {
        // language=glsl
        R"glsl(
            #extension GL_EXT_mesh_shader : require

            layout(triangles, max_vertices = 3, max_primitives = 1) out;
            layout(location = 0) out vec3 out_colors[];

            const vec2 positions[3] = vec2[] (vec2(-0.6, 0.6), vec2(0.6, 0.6), vec2(0, -0.6));
            const vec3    colors[3] = vec3[] (vec3(1, 0, 0),   vec3(0, 1, 0),  vec3(0, 0, 1));

            layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
            void main() {
                SetMeshOutputsEXT(3, 1);
                for (int i = 0; i < 3; ++i) {
                    gl_MeshVerticesEXT[i].gl_Position = vec4(positions[i], 0, 1);
                    out_colors[i] = colors[i];
                }
                gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);
            }
        )glsl"
    });

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

    while (app.IsRunning()) {

        fence.Wait();
        queue.Acquire({swapchain}, {fence});
        cmd_pool.Reset();
        auto cmd = cmd_pool.Begin();

        cmd.BeginRendering({{}, swapchain.GetExtent()}, {swapchain.GetCurrent()});
        cmd.ClearColor(0, Vec4(Vec3(0.1f), 1.f), swapchain.GetExtent());
        cmd.ResetGraphicsState();
        cmd.SetViewports({{{}, Vec2I(swapchain.GetExtent())}}, true);
        cmd.SetBlendState({false});
        cmd.BindShaders({task_shader, mesh_shader, fragment_shader});
        cmd.DrawMeshTasks(Vec3U(1));
        cmd.EndRendering();

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        app.WaitEvents();
        app.PollEvents();
    }
}