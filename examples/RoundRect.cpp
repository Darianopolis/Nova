#include <nova/gui/Draw2DRect.slang>

#include "main/Main.hpp"

#include <nova/gpu/RHI.hpp>
#include <nova/window/Window.hpp>

NOVA_EXAMPLE(RoundRect, "round-rect")
{
    constexpr u32 size = 1440;

    constexpr u32 num_quads = 65'536;
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

    auto swapchain = nova::Swapchain::Create(context, window,
        nova::ImageUsage::ColorAttach
        | nova::ImageUsage::TransferDst,
        nova::PresentMode::Mailbox);
    NOVA_DEFER(&) { swapchain.Destroy(); };

    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);

    // Quad data

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> size_dist{20.f, 30.f};
    std::uniform_real_distribution<float> rotation_dist{0.f, 2.f * glm::pi<float>()};
    std::uniform_real_distribution<float> position_dist{-0.5f * size, 0.5f * size};
    std::uniform_real_distribution<float> unorm_dist{0.f, 1.f};

    auto quads = nova::Buffer::Create(context, num_quads * sizeof(nova::draw::Rectangle),
        nova::BufferUsage::Storage,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_DEFER(&) { quads.Destroy(); };
    for (u32 i = 0; i < num_quads; ++i) {
        Vec2 half_extent = Vec2(size_dist(rng), size_dist(rng));
        auto get_color = [&]{ return Vec4(unorm_dist(rng),unorm_dist(rng),unorm_dist(rng),unorm_dist(rng)); };
        quads.Set<nova::draw::Rectangle>(nova::draw::Rectangle {
            .center_color = {get_color()},
            .border_color = {get_color()},
            .center_pos = Vec2(position_dist(rng), position_dist(rng)) * 0.99f,
            .half_extent = half_extent,
            .rotation = rotation_dist(rng),
            .corner_radius = glm::min(half_extent.x, half_extent.y) * unorm_dist(rng),
            .border_width = glm::min(half_extent.x, half_extent.y) * unorm_dist(rng),
        }, i);
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

    // Shaders

    auto batch_vertex_shader    = nova::Shader::Create(context, nova::ShaderLang::Slang, nova::ShaderStage::Vertex, "Vertex", "nova/ui/Draw2DRect.slang");
    NOVA_DEFER(&) { batch_vertex_shader.Destroy(); };

    auto fragment_shader        = nova::Shader::Create(context, nova::ShaderLang::Slang, nova::ShaderStage::Fragment, "Fragment", "nova/ui/Draw2DRect.slang");
    NOVA_DEFER(&) { fragment_shader.Destroy(); };

    // Variables

    auto last_time = std::chrono::steady_clock::now();
    u32 frames = 0;

    u32 fif = 1;
    std::array<nova::SyncPoint, 2> wait_values;

    // Draw loop

    NOVA_DEFER(&) { queue.WaitIdle(); };
    while (app.ProcessEvents()) {

        fif = 1 - fif;
        wait_values[fif].Wait();

        queue.Acquire({swapchain});

        // Debug output statistics
        frames++;
        auto new_time = std::chrono::steady_clock::now();
        if (new_time - last_time > 1s) {
            nova::Log("Frametime = {} ({} fps)", nova::DurationToString(std::chrono::duration<float>(1.f / frames)), frames);

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
        cmd.SetBlendState({true});

        cmd.PushConstants(PushConstants {
            .inv_half_extent = 2.f / Vec2(size),
            .rects = (const nova::draw::Rectangle*)quads.DeviceAddress(),
        });

        cmd.BindShaders({batch_vertex_shader, fragment_shader});

        cmd.BindIndexBuffer(indices, nova::IndexType::U32);
        cmd.DrawIndexed(num_indices, 1, 0, 0, 0);

        cmd.EndRendering();

        cmd.Present(swapchain);
        wait_values[fif] = queue.Submit({cmd}, {});
        queue.Present({swapchain}, {});
    }
}
