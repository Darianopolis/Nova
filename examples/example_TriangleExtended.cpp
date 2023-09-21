#include "example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

struct PushConstants
{
    Vec2U uniforms;
    u64   vertices;

    static constexpr std::array Layout {
        nova::Member("uniforms", nova::UniformBufferType("Uniforms", true)),
        nova::Member("vertices", nova::BufferReferenceType("Vertex", true)),
    };
};

struct Uniforms
{
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

NOVA_EXAMPLE(TriangleBuffered, "tri-extended")
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

    [[maybe_unused]] auto generatedShaderCode = R"glsl(
#version 460
// -- Extension --
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference2 : enable
// -- Uniforms --
struct Uniforms {
    vec3 offset;
};
layout(set = 0, binding = 0, scalar) readonly uniform _2_ { Uniforms data[1024]; } Uniforms_readonly_uniform_buffer[];
// -- Vertices --
struct Vertex {
    vec3 position;
    vec3 color;
};
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Vertex_readonly_buffer_reference { Vertex get; };
// -- PushConstants --
layout(push_constant, scalar) uniform _10_ {
    uvec2 uniforms;
    Vertex_readonly_buffer_reference vertices;
} pc;
// -- Outputs --
layout(location = 0) out vec3 color;
// -- Entry --
void main() {
    uvec2 u = pc.uniforms;
    Vertex_readonly_buffer_reference v = pc.vertices[gl_VertexIndex];
    color = v.get.color;
    gl_Position = vec4(v.get.position + Uniforms_readonly_uniform_buffer[u.x].data[u.y].offset,1);
}
    )glsl";

    auto vertexShader = nova::Shader::Create2(context, nova::ShaderStage::Vertex, {
        nova::shader::Structure("Uniforms", Uniforms::Layout),
        nova::shader::Structure("Vertex", Vertex::Layout),
        nova::shader::PushConstants("pc", PushConstants::Layout),
        nova::shader::Output("color", nova::ShaderVarType::Vec3),
        nova::shader::Fragment(R"glsl(
            fn main() {
                ref u = pc.uniforms;
                ref v = pc.vertices[gl_VertexIndex];
                color = v.color;
                gl_Position = vec4(v.position + u.offset, 1);
            }
        )glsl"),
    });
    NOVA_CLEANUP(&) { vertexShader.Destroy(); };

    auto fragmentShader = nova::Shader::Create2(context, nova::ShaderStage::Fragment, {
        nova::shader::Input("inColor", nova::ShaderVarType::Vec3),
        nova::shader::Output("fragColor", nova::ShaderVarType::Vec4),
        nova::shader::Fragment(R"glsl(
            fn main() {
                fragColor = vec4(inColor, 1);
            }
        )glsl"),
    });
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