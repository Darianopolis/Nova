#include "example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

void example_Triangle()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1200, "Nova - Triangle", nullptr, nullptr);
    NOVA_CLEANUP(&) {
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    auto context = nova::Context::Create({
        .debug = false,
    });
    NOVA_CLEANUP(&) { context.Destroy(); };

    auto swapchain = nova::Swapchain::Create(context, glfwGetWin32Window(window),
        nova::TextureUsage::ColorAttach
        | nova::TextureUsage::TransferDst,
        nova::PresentMode::Fifo);
    NOVA_CLEANUP(&) { swapchain.Destroy(); };

    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto cmdPool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);
    auto state = nova::CommandState::Create(context);
    NOVA_CLEANUP(&) {
        cmdPool.Destroy();
        fence.Destroy();
        state.Destroy();
    };

    // Vertex data

    struct Vertex
    {
        Vec3 position;
        Vec3 color;
    };

    auto vertices = nova::Buffer::Create(context, 3 * sizeof(Vertex),
        nova::BufferUsage::Storage,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_CLEANUP(&) { vertices.Destroy(); };
    vertices.Set<Vertex>({ 
        {{ -0.6f, 0.6f, 0.f }, { 1.f, 0.f, 0.f }}, 
        {{  0.6f, 0.6f, 0.f }, { 0.f, 1.f, 0.f }}, 
        {{  0.f, -0.6f, 0.f }, { 0.f, 0.f, 1.f }} 
    });

    auto indices = nova::Buffer::Create(context, 6 * 4,
        nova::BufferUsage::Index,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_CLEANUP(&) { indices.Destroy(); };
    indices.Set<u32>({0, 1, 2});

    // Pipeline

    auto descLayout = nova::DescriptorSetLayout::Create(context, {
        nova::binding::UniformBuffer("ubo", {{"pos", nova::ShaderVarType::Vec3}}),
    });
    auto pcRange = nova::PushConstantRange("pc", {{"vertexVA", nova::ShaderVarType::U64},});
    auto pipelineLayout = nova::PipelineLayout::Create(context, {pcRange}, {descLayout}, nova::BindPoint::Graphics);
    NOVA_CLEANUP(&) {
        descLayout.Destroy();
        pipelineLayout.Destroy();
    };

    // UBO descriptor set

    auto ubo = nova::Buffer::Create(context, sizeof(Vec3),
        nova::BufferUsage::Uniform,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_CLEANUP(&) { ubo.Destroy(); };
    ubo.Set<Vec3>({{0.f, 0.f, 0.f}});

    auto set = nova::DescriptorSet::Create(descLayout);
    NOVA_CLEANUP(&) { set.Destroy(); };
    set.WriteUniformBuffer(0, ubo);

    // Create vertex shader

    auto vertexShader = nova::Shader::Create(context, nova::ShaderStage::Vertex, {
        nova::shader::Structure("Vertex", {
            {"position", nova::ShaderVarType::Vec3},
            {"color", nova::ShaderVarType::Vec3},
        }),
        nova::shader::Layout(pipelineLayout),

        nova::shader::Output("color", nova::ShaderVarType::Vec3),
        nova::shader::Kernel(R"glsl(
            Vertex v = Vertex_BR(pc.vertexVA)[gl_VertexIndex];
            color = v.color;
            gl_Position = vec4(v.position + ubo.pos, 1);
        )glsl"),
    });
    NOVA_CLEANUP(&) { vertexShader.Destroy(); };

    auto fragmentShader = nova::Shader::Create(context, nova::ShaderStage::Fragment, {
        nova::shader::Input("inColor", nova::ShaderVarType::Vec3),
        nova::shader::Output("fragColor", nova::ShaderVarType::Vec4),
        nova::shader::Kernel("fragColor = vec4(inColor, 1);"),
    });
    NOVA_CLEANUP(&) { fragmentShader.Destroy(); };

    // Draw

    NOVA_CLEANUP(&) { fence.Wait(); };
    while (!glfwWindowShouldClose(window))
    {
        fence.Wait();
        queue.Acquire({swapchain}, {fence});

        cmdPool.Reset();
        auto cmd = cmdPool.Begin(state);

        cmd.BeginRendering({{}, swapchain.GetExtent()}, {swapchain.GetCurrent()});
        cmd.ClearColor(0, Vec4(Vec3(0.1f), 1.f), swapchain.GetExtent());
        cmd.SetGraphicsState(pipelineLayout, {vertexShader, fragmentShader}, {});
        cmd.PushConstants(pipelineLayout, 0, sizeof(u64), nova::Temp(vertices.GetAddress()));
        cmd.BindDescriptorSets(pipelineLayout, 0, {set});
        cmd.BindIndexBuffer(indices, nova::IndexType::U32);
        cmd.DrawIndexed(3, 1, 0, 0, 0);
        cmd.EndRendering();

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        glfwWaitEvents();
    }
}