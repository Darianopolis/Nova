#include "example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/vulkan/glsl/nova_VulkanGlsl.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

struct PushConstants
{
    Vec2U uniforms;
    u64 vertices;
};

struct Uniforms
{
    Vec3 offset;
};

struct Vertex
{
    Vec3 position;
    Vec3    color;
};

NOVA_EXAMPLE(TriangleBuffered, "tri-ext")
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1200, "Nova - Triangle Extended", nullptr, nullptr);
    NOVA_CLEANUP(&) { glfwTerminate(); };

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
    auto heap = nova::DescriptorHeap::Create(context, 1);
    NOVA_CLEANUP(&) {
        cmdPool.Destroy();
        fence.Destroy();
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

    heap.WriteUniformBuffer(0, ubo);

    // Shaders

    auto vertexShader = nova::Shader::Create(context, nova::ShaderStage::Vertex, "main",
        nova::glsl::Compile(nova::ShaderStage::Vertex, "main", "", {R"glsl(
            #version 460
            #extension GL_EXT_scalar_block_layout  : require
            #extension GL_EXT_buffer_reference2    : require
            #extension GL_EXT_nonuniform_qualifier : require

            layout(set = 0, binding = 0, scalar) readonly uniform Uniforms_ { vec3 offset; } Uniforms[];

            layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Vertex {
                vec3 position;
                vec3 color;
            };

            layout(push_constant, scalar) uniform pc_ {
                uint   uniforms;
                Vertex vertices;
            } pc;

            layout(location = 0) out vec3 color;

            void main() {
                Vertex v = pc.vertices[gl_VertexIndex];
                color = v.color;
                gl_Position = vec4(v.position + Uniforms[pc.uniforms].offset, 1);
            }
        )glsl"}));
    NOVA_CLEANUP(&) { vertexShader.Destroy(); };

    auto fragmentShader = nova::Shader::Create(context, nova::ShaderStage::Fragment, "main",
        nova::glsl::Compile(nova::ShaderStage::Fragment, "main", "", {R"glsl(
            #version 460

            layout(location = 0) in vec3 inColor;
            layout(location = 0) out vec4 fragColor;

            void main() {
                fragColor = vec4(inColor, 1);
            }
        )glsl"}));
    NOVA_CLEANUP(&) { fragmentShader.Destroy(); };

    // Draw

    NOVA_CLEANUP(&) { fence.Wait(); };
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

        cmd.PushConstants(PushConstants {
            .uniforms = Vec2U(0, 0),
            .vertices = vertices.GetAddress()
        });
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