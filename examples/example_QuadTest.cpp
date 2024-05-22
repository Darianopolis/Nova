#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_ToString.hpp>
#include <nova/rhi/nova_RHI.hpp>

#include <nova/window/nova_Window.hpp>

struct PushConstants
{
    u64 quads;
    Vec2 quad_size;
};

struct Quad
{
    Vec2 position;
};

NOVA_EXAMPLE(QuadTest, "quad-test")
{
    constexpr u32 size = 2048;
    constexpr u32 quad_side_count = size / 1;
    constexpr u32 num_quads = quad_side_count * quad_side_count;
    constexpr f32 inv_half_size = 2.f / quad_side_count;
    constexpr u32 num_indices = num_quads * 6;

    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Quad Test")
        .SetSize({ size, size }, nova::WindowPart::Client)
        .Show(true);

    auto context = nova::Context::Create({
        .debug = false,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    auto swapchain = nova::Swapchain::Create(context, window.NativeHandle(),
        nova::ImageUsage::ColorAttach
        | nova::ImageUsage::TransferDst,
        nova::PresentMode::Mailbox);
    NOVA_DEFER(&) { swapchain.Destroy(); };

    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);

    // Quad data

    auto quads = nova::Buffer::Create(context, num_quads * sizeof(Quad),
        nova::BufferUsage::Storage,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_DEFER(&) { quads.Destroy(); };
    for (u32 y = 0; y < quad_side_count; ++y) {
        for (u32 x = 0; x < quad_side_count; ++x) {
            quads.Set<Quad>(Quad({x * inv_half_size - 1, y * inv_half_size - 1}), x * quad_side_count + y);
        }
    }

    // Indices

    auto indices = nova::Buffer::Create(context, num_indices * sizeof(u32),
        nova::BufferUsage::Index,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_DEFER(&) { indices.Destroy(); };
    for (u32 i = 0; i < num_indices - 6; i += 6) {
        indices.Set<u32>({
            i + 0, i + 1, i + 2,
            i + 1, i + 4, i + 2,
        }, i);
    }

    std::array<nova::SyncPoint, 2> wait_values;

    // Shaders

    auto vertex_preamble =
        R"glsl(
#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_buffer_reference2    : require
#extension GL_EXT_nonuniform_qualifier : require

const vec2 positions[6] = vec2[] (
    vec2(0, 1), vec2(1, 1), vec2(0, 0),
    vec2(1, 1), vec2(1, 0), vec2(0, 0)
);

layout(buffer_reference, scalar, buffer_reference_align = 8) readonly buffer Quad {
    vec2 position;
};

layout(push_constant, scalar) uniform pc_ {
    Quad quads;
    vec2 quad_size;
} pc;

layout(location = 0) out vec2 uv;
        )glsl";

    // Vertex shader for batched (all quads in one instance)

    auto batch_vertex_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::Vertex, "main", "", {
        vertex_preamble,
        R"glsl(
void main() {
    Quad q = pc.quads[gl_VertexIndex / 6];
    vec2 local_pos = positions[gl_VertexIndex % 6];
    gl_Position = vec4(q.position + (local_pos * pc.quad_size), 0, 1);
    uv = (q.position * 0.5) + 1.0;
}
        )glsl"
    });
    NOVA_DEFER(&) { batch_vertex_shader.Destroy(); };

    // Vertex shader for instanced (one quad per instance)

    auto instance_vertex_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::Vertex, "main", "", {
        vertex_preamble,
        R"glsl(
void main() {
    Quad q = pc.quads[gl_InstanceIndex];
    vec2 local_pos = positions[gl_VertexIndex];
    gl_Position = vec4(q.position + (local_pos * pc.quad_size), 0, 1);
    uv = (q.position * 0.5) + 1.0;
}
        )glsl"
    });
    NOVA_DEFER(&) { instance_vertex_shader.Destroy(); };

    // Fragment shader

    auto fragment_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::Fragment, "main", "", {
        R"glsl(
layout(location = 0) in vec2 in_uv;
// layout(location = 0) out vec4 frag_color;

void main() {
    // frag_color = vec4(in_uv, 0, 1);
}
        )glsl"
    });
    NOVA_DEFER(&) { fragment_shader.Destroy(); };

    bool indexed = true;
    bool instanced = false;

    app.AddCallback([&](const nova::AppEvent& e) {
        if (e.type == nova::EventType::Input) {
            auto vk = app.ToVirtualKey(e.input.channel);
            if (e.input.pressed) {
                if (vk == nova::VirtualKey::Q) indexed = !indexed;
                if (vk == nova::VirtualKey::E) instanced = !instanced;
            }
        }
    });

    // Draw

    auto last_time = std::chrono::steady_clock::now();
    u32 frames = 0;
    u32 fif = 1;

    NOVA_DEFER(&) { queue.WaitIdle(); };
    while (app.ProcessEvents()) {

        fif = 1 - fif;
        wait_values[fif].Wait();

        queue.Acquire({swapchain});

        // Debug output statistics
        frames++;
        auto new_time = std::chrono::steady_clock::now();
        if (new_time - last_time > 1s) {
            nova::Log("Indexed = {}\tInstanced = {}\tFrametime = {} ({} fps)", indexed, instanced, nova::DurationToString(std::chrono::duration<float>(1.f / frames)), frames);

            last_time = std::chrono::steady_clock::now();
            frames = 0;
        }

        auto cmd = queue.Begin();

        cmd.BeginRendering({
            .region = {{}, swapchain.Extent()},
            .color_attachments = {swapchain.Target()}
        });

        cmd.ResetGraphicsState();
        cmd.SetViewports({{{}, Vec2I(swapchain.Extent())}}, true);
        cmd.SetBlendState({false});

        cmd.PushConstants(PushConstants {
            .quads = quads.DeviceAddress(),
            .quad_size = Vec2(inv_half_size),
        });

        if (instanced) {
            cmd.BindShaders({instance_vertex_shader, fragment_shader});
        } else {
            cmd.BindShaders({batch_vertex_shader, fragment_shader});
        }

        const u32 num_indices_per_instance = instanced ? 6 : num_indices;
        const u32 num_instances            = instanced ? num_quads : 1;

        if (indexed) {
            cmd.BindIndexBuffer(indices, nova::IndexType::U32);
            cmd.DrawIndexed(num_indices_per_instance, num_instances, 0, 0, 0);
        } else {
            cmd.Draw(num_indices_per_instance, num_instances, 0, 0);
        }

        cmd.EndRendering();

        cmd.Present(swapchain);
        wait_values[fif] = queue.Submit({cmd}, {});
        queue.Present({swapchain}, {});
    }
}