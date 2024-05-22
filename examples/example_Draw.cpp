#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Timer.hpp>
#include <nova/rhi/nova_RHI.hpp>
#include <nova/ui/nova_Draw2D.hpp>
#include <nova/window/nova_Window.hpp>
#include <nova/vfs/nova_VirtualFilesystem.hpp>

#include <nova/core/win32/nova_Win32Include.hpp>
#pragma warning(push)
#pragma warning(disable: 4263)
#include "dcomp.h"
#pragma warning(pop)

#include <stb_image.h>

NOVA_EXAMPLE(Draw, "draw")
{
    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };

    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Draw")
        .SetDecorate(false)
        .SetTransparent(true, { 0, 0, 0 })
        .Show(true);

    auto context = nova::Context::Create({
        .debug = false,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    auto swapchain = nova::Swapchain::Create(context, window.NativeHandle(),
        nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach,
        nova::PresentMode::Fifo);
    NOVA_DEFER(&) { swapchain.Destroy(); };

    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);

// -----------------------------------------------------------------------------

    nova::draw::Draw2D im_draw{ context };

// -----------------------------------------------------------------------------

    nova::Image image;
    {
        i32 w, h, c;
        auto data = stbi_load("assets/textures/statue.jpg", &w, &h, &c, STBI_rgb_alpha);
        NOVA_DEFER(&) { stbi_image_free(data); };

        image = nova::Image::Create(context,
            Vec3U(u32(w), u32(h), 0),
            nova::ImageUsage::Sampled,
            nova::Format::RGBA8_UNorm);

        image.Set({}, {u32(w), u32(h), 1}, data);
        image.Transition(nova::ImageLayout::Sampled);
    }

// -----------------------------------------------------------------------------

    auto size = app.PrimaryDisplay().Size();
    std::cout << "Monitor size = " << size.x << ", " << size.y << '\n';

    auto font = im_draw.LoadFont(nova::vfs::Load("fonts/arial.ttf"), 20.f);

    nova::draw::Rectangle box1 {
        .center_color = { 1.f, 0.f, 0.f, 0.5f },
        .border_color = { 0.2f, 0.2f, 0.2f, 1.f },
        .center_pos = { size.x * 0.25f, size.y * 0.25f },
        .half_extent = { 100.f, 200.f },
        .corner_radius = 15.f,
        .border_width = 5.f,

        .tex_tint = { 1.f, 1.f, 1.f, 1.f },
        .tex_handle = {image.Descriptor(), im_draw.default_sampler.Descriptor()},
        .tex_center_pos = { 0.5f, 0.5f },
        .tex_half_extent = { 0.5f, 1.f },
    };

    nova::draw::Rectangle box2 {
        .center_color = { 0.f, 1.f, 0.f, 0.5f },
        .border_color = { 0.4f, 0.4f, 0.4f, 1.f },
        .center_pos = { size.x * 0.5f, size.y * 0.5f },
        .half_extent = { 100.f, 100.f },
        .corner_radius = 15.f,
        .border_width = 10.f,

        .tex_tint = { 1.f, 1.f, 1.f, 1.f },
        .tex_handle = {image.Descriptor(), im_draw.default_sampler.Descriptor()},
        .tex_center_pos = { 0.5f, 0.5f },
        .tex_half_extent = { 0.5f, 0.5f },
    };

    nova::draw::Rectangle box3 {
        .center_color = { 0.f, 0.f, 1.f, 0.5f },
        .border_color = { 0.6f, 0.6f, 0.6f, 1.f },
        .center_pos = { size.x * 0.75f, size.y * 0.75f },
        .half_extent = { 200.f, 100.f },
        .corner_radius = 25.f,
        .border_width = 10.f,

        .tex_tint = { 1.f, 1.f, 1.f, 1.f },
        .tex_handle = {image.Descriptor(), im_draw.default_sampler.Descriptor()},
        .tex_center_pos = { 0.5f, 0.5f },
        .tex_half_extent = { 1.f, 0.5f },
    };

    bool redraw = true;
    bool skip_update = false;

    auto last_frame = std::chrono::steady_clock::now();

    NOVA_DEFER(&) { queue.WaitIdle(); };
    while (app.ProcessEvents()) {

// -----------------------------------------------------------------------------

        if (!skip_update) {
            auto MoveBox = [&](nova::draw::Rectangle& box, nova::VirtualKey left, nova::VirtualKey right, nova::VirtualKey up, nova::VirtualKey down) {
                float speed = 10.f;
                if (app.IsVirtualKeyDown(left))  { box.center_pos.x -= speed; redraw = true; }
                if (app.IsVirtualKeyDown(right)) { box.center_pos.x += speed; redraw = true; }
                if (app.IsVirtualKeyDown(up))    { box.center_pos.y -= speed; redraw = true; }
                if (app.IsVirtualKeyDown(down))  { box.center_pos.y += speed; redraw = true; }
            };

            MoveBox(box1, nova::VirtualKey::A, nova::VirtualKey::D, nova::VirtualKey::W, nova::VirtualKey::S);
            MoveBox(box2, nova::VirtualKey::J, nova::VirtualKey::L, nova::VirtualKey::I, nova::VirtualKey::K);
            MoveBox(box3, nova::VirtualKey::Left, nova::VirtualKey::Right, nova::VirtualKey::Up, nova::VirtualKey::Down);
        }
        else {
            redraw = true;
        }

        skip_update = false;

        if (!redraw) {
            app.WaitForEvents();
            skip_update = true;
        }
        redraw = false;

// -----------------------------------------------------------------------------

        im_draw.Reset();

        im_draw.DrawRect(box1);
        im_draw.DrawRect(box2);
        im_draw.DrawRect(box3);

        im_draw.DrawString(
            "C:/Program Files (x86)/Steam/steamapps/common/BeamNG.drive/BeamNG.drive.exe",
            Vec2(size.x * 0.25f, size.y * 0.4f),
            *font);

// -----------------------------------------------------------------------------

        // Record frame

        queue.WaitIdle();

        auto cmd = queue.Begin();

        // Update window size, record primary buffer and present

        // TODO: Investigate integratino of this and/or a manual swapchain implementation?
        // DCompositionWaitForCompositorClock(0, nullptr, INFINITE);

        window.SetSize({ im_draw.Bounds().Width(), im_draw.Bounds().Height() }, nova::WindowPart::Client);

        queue.Acquire({swapchain});

        Vec3 color = { 0.3f, 0.2f, 0.5f };
        f32 alpha = 0.4f;
        cmd.ClearColor(swapchain.Target(), Vec4(color * alpha, alpha));

        im_draw.Record(cmd, swapchain.Target());

        cmd.Present(swapchain);

        queue.Submit({cmd}, {}).Wait();
        window.SetPosition({ im_draw.Bounds().min.x, im_draw.Bounds().min.y }, nova::WindowPart::Client);
        queue.Present({swapchain}, {});
    }
}
