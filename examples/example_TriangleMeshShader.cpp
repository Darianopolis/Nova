#include "main/example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>
#include <nova/window/nova_Window.hpp>

NOVA_EXAMPLE(TriangleMeshShader, "tri-mesh")
{
    nova::rhi::Init({});

    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Triangle Mesh Shader")
        .SetSize({ 1920, 1080 }, nova::WindowPart::Client)
        .Show(true);

    auto swapchain = nova::Swapchain::Create(window,
        nova::ImageUsage::ColorAttach
        | nova::ImageUsage::TransferDst,
        nova::PresentMode::Fifo);
    NOVA_DEFER(&) { swapchain.Destroy(); };
    auto queue = nova::Queue::Get(nova::QueueFlags::Graphics, 0);

    auto task_shader = nova::Shader::Create(nova::ShaderLang::Slang, nova::ShaderStage::Task, "Task", "example_TriangleMeshShader.slang");
    NOVA_DEFER(&) { task_shader.Destroy(); };
    auto mesh_shader = nova::Shader::Create(nova::ShaderLang::Slang, nova::ShaderStage::Mesh, "Mesh", "example_TriangleMeshShader.slang");
    NOVA_DEFER(&) { mesh_shader.Destroy(); };
    auto fragment_shader = nova::Shader::Create(nova::ShaderLang::Slang, nova::ShaderStage::Fragment, "Fragment", "example_TriangleMeshShader.slang");
    NOVA_DEFER(&) { fragment_shader.Destroy(); };

    NOVA_DEFER(&) { queue.WaitIdle(); };
    while (app.ProcessEvents()) {

        queue.WaitIdle();
        queue.Acquire({swapchain}, {});
        auto cmd = queue.Begin();

        cmd.BeginRendering({
            .region = {{}, swapchain.Extent()},
            .color_attachments = {swapchain.Target()}
        });
        cmd.ClearColor(0, Vec4(Vec3(0.1f), 1.f), swapchain.Extent());
        cmd.ResetGraphicsState();
        cmd.SetViewports({{{}, Vec2I(swapchain.Extent())}}, true);
        cmd.SetBlendState({false});
        cmd.BindShaders({task_shader, mesh_shader, fragment_shader});
        cmd.DrawMeshTasks(Vec3U(1));
        cmd.EndRendering();

        cmd.Present(swapchain);
        queue.Submit({cmd}, {});
        queue.Present({swapchain}, {});

        app.WaitForEvents();
    }
}