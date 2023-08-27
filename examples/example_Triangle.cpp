#include "example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

struct PushConstants {
    u64 vertexVA;
    u32 uniformUBO;

    static constexpr std::array Layout {
        nova::Member("vertexVA",   nova::ShaderVarType::U64),
        nova::Member("uniformUBO", nova::ShaderVarType::U32),
    };
};

struct Uniforms {
    Vec3 offset;

    static constexpr std::array Layout {
        nova::Member("offset", nova::ShaderVarType::Vec3),
    };
};

struct Vertex
{
    Vec3 position;
    Vec3 color;

    static constexpr std::array Layout {
        nova::Member("position", nova::ShaderVarType::Vec3),
        nova::Member("color",    nova::ShaderVarType::Vec3),
    };
};

NOVA_EXAMPLE(tri)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1200, "Nova - Triangle", nullptr, nullptr);
    NOVA_CLEANUP(&) {
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    auto context = nova::Context::Create({
        .debug = true,
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
    auto heap = nova::DescriptorHeap::Create(context);
    NOVA_CLEANUP(&) {
        cmdPool.Destroy();
        fence.Destroy();
        state.Destroy();
        heap.Destroy();
    };

    // Vertex data

    auto vertices = nova::Buffer::Create(context, 3 * sizeof(Vertex),
        nova::BufferUsage::Storage,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_CLEANUP(&) { vertices.Destroy(); };
    vertices.Set<Vertex>({
        {{ -0.6f, 0.6f, 0.f }, { 1.f, 0.f, 0.f }},
        {{  0.6f, 0.6f, 0.f }, { 0.f, 1.f, 0.f }},
        {{  0.f, -0.6f, 0.f }, { 0.f, 0.f, 1.f }}
    });

    // Indices

    auto indices = nova::Buffer::Create(context, 6 * 4,
        nova::BufferUsage::Index,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_CLEANUP(&) { indices.Destroy(); };
    indices.Set<u32>({0, 1, 2});

    // Uniforms

    auto ubo = nova::Buffer::Create(context, sizeof(Vec3),
        nova::BufferUsage::Uniform,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_CLEANUP(&) { ubo.Destroy(); };
    ubo.Set<Vec3>({{0.f, 0.25f, 0.f}});

    auto uboHandle = heap.Acquire(nova::DescriptorType::Uniform);
    heap.WriteUniformBuffer(uboHandle, ubo);

    // Shaders

    auto vertexShader = nova::Shader::Create(context, nova::ShaderStage::Vertex, {
        nova::shader::Structure("Vertex", Vertex::Layout),
        nova::shader::Structure("Uniforms", Uniforms::Layout),
        nova::shader::PushConstants("pc", PushConstants::Layout),
        nova::shader::Output("color", nova::ShaderVarType::Vec3),
        nova::shader::Kernel(R"glsl(
            Uniforms u = nova::UniformBuffer<Uniforms>[pc.uniformUBO].data[0];
            Vertex   v = nova::BufferReference<Vertex>(pc.vertexVA).data[gl_VertexIndex];
            color = v.color;
            gl_Position = vec4(v.position + u.offset, 1);
        )glsl"),
    });
    NOVA_CLEANUP(&) { vertexShader.Destroy(); };

    auto fragmentShader = nova::Shader::Create(context, nova::ShaderStage::Fragment, {
        nova::shader::Input("inColor", nova::ShaderVarType::Vec3),
        nova::shader::Output("fragColor", nova::ShaderVarType::Vec4),
        nova::shader::Kernel(R"glsl(
            fragColor = vec4(inColor, 1);
        )glsl"),
    });
    NOVA_CLEANUP(&) { fragmentShader.Destroy(); };

    // Draw

    NOVA_CLEANUP(&) { fence.Wait(); };
    while (!glfwWindowShouldClose(window)) {
        fence.Wait();
        queue.Acquire({swapchain}, {fence});

        cmdPool.Reset();
        auto cmd = cmdPool.Begin(state);

        cmd.BeginRendering({{}, swapchain.GetExtent()}, {swapchain.GetCurrent()});
        cmd.ClearColor(0, Vec4(Vec3(0.1f), 1.f), swapchain.GetExtent());
        cmd.SetGraphicsState({vertexShader, fragmentShader}, {});
        cmd.PushConstants(0, sizeof(PushConstants), nova::Temp(PushConstants {
            .vertexVA = vertices.GetAddress(),
            .uniformUBO = uboHandle.ToShaderUInt(),
        }));
        cmd.BindDescriptorHeap(nova::BindPoint::Graphics, heap);
        cmd.BindIndexBuffer(indices, nova::IndexType::U32);
        cmd.DrawIndexed(3, 1, 0, 0, 0);
        cmd.EndRendering();

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        glfwWaitEvents();
    }
}