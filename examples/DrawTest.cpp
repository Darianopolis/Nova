#include "main/Main.hpp"

#include <nova/gpu/RHI.hpp>
#include <nova/gui/Draw2D.hpp>
#include <nova/window/Window.hpp>
#include <nova/filesystem/VirtualFileSystem.hpp>
#include <nova/image/Image.hpp>

NOVA_EXAMPLE(Draw, "draw")
{
    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };

    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Draw")
        .SetDecorate(false)
        .SetTransparency(nova::TransparencyMode::PerPixel)
        .Show(true);

    auto context = nova::Context::Create({
        .debug = true,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    auto swapchain = nova::Swapchain::Create(context, window,
        nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach,
        nova::PresentMode::Layered,
        nova::SwapchainFlags::PreMultipliedAlpha);
    NOVA_DEFER(&) { swapchain.Destroy(); };

    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);

// -----------------------------------------------------------------------------

    nova::draw::Draw2D im_draw{ context };

// -----------------------------------------------------------------------------

    nova::Image image;
    {
        nova::ImageDescription desc;
        nova::ImageLoadData src;
        nova::Image_Load(&desc, &src, "assets/textures/statue.jpg");
        NOVA_DEFER(&) { src.Destroy(); };

        nova::ImageDescription target_desc{ desc };
        target_desc.is_signed = false;
        target_desc.format = nova::ImageFormat::RGBA8;
        nova::ImageAccessor target_accessor(target_desc);
        std::vector<b8> data(target_accessor.GetSize());
        nova::Image_Copy(desc, src.data, target_accessor, data.data());

        image = nova::Image::Create(context,
            Vec3U(desc.width, desc.height, 0),
            nova::ImageUsage::Sampled,
            nova::Format::RGBA8_UNorm);

        image.Set({}, {desc.width, desc.height, 1}, data.data());
        image.Transition(nova::ImageLayout::Sampled);
    }
    NOVA_DEFER(&) { image.Destroy(); };

// -----------------------------------------------------------------------------

    auto size = app.PrimaryDisplay().Size();
    std::cout << "Monitor size = " << size.x << ", " << size.y << '\n';

    auto font_size = 20.f;
    auto font = im_draw.LoadFont(nova::vfs::Load("arial.ttf"));

    nova::draw::Rectangle box1 {
        .center_color = { 1.f, 0.f, 0.f, 0.5f },
        .border_color = { 0.2f, 0.2f, 0.2f, 1.f },
        .center_pos = { size.x * 0.25f, size.y * 0.25f },
        .half_extent = { 100.f, 200.f },
        .corner_radius = 15.f,
        .border_width = 5.f,

        .tex_handle = {image.Descriptor(), im_draw.default_sampler.Descriptor()},
        .tex_tint = { 1.f, 1.f, 1.f, 1.f },
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

        .tex_handle = {image.Descriptor(), im_draw.default_sampler.Descriptor()},
        .tex_tint = { 1.f, 1.f, 1.f, 1.f },
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

        .tex_handle = {image.Descriptor(), im_draw.default_sampler.Descriptor()},
        .tex_tint = { 1.f, 1.f, 1.f, 1.f },
        .tex_center_pos = { 0.5f, 0.5f },
        .tex_half_extent = { 1.f, 0.5f },
    };

    bool redraw = true;
    bool skip_update = false;

    auto last_frame = std::chrono::steady_clock::now();

    auto last_time = std::chrono::steady_clock::now();
    auto frames = 0;

    NOVA_DEFER(&) { queue.WaitIdle(); };
    while (app.ProcessEvents()) {
        if (app.IsVirtualKeyDown(nova::VirtualKey::Escape)) {
            window.Destroy();
            continue;
        }

        auto new_time = std::chrono::steady_clock::now();

        float delta_time = std::chrono::duration_cast<std::chrono::duration<float>>(new_time - last_frame).count();
        last_frame = new_time;

        // Debug output statistics
        frames++;
        if (new_time - last_time > 1s) {
            nova::Log("Frametime   = {} ({} fps)"
                    "\nAllocations = {:3} (+ {} /s)",
                nova::DurationToString((new_time - last_time) / frames), frames, nova::rhi::stats::AllocationCount.load(),
                nova::rhi::stats::NewAllocationCount.exchange(0));
            f64 divisor = 1000.0 * frames;
            nova::Log("submit :: clear     = {:.2f}"
                    "\nsubmit :: adapting1 = {:.2f}"
                    "\nsubmit :: adapting2 = {:.2f}"
                    "\npresent             = {:.2f}\n",
                nova::rhi::stats::TimeSubmitting.exchange(0) / divisor,
                nova::rhi::stats::TimeAdaptingFromAcquire.exchange(0)  / divisor,
                nova::rhi::stats::TimeAdaptingToPresent.exchange(0)  / divisor,
                nova::rhi::stats::TimePresenting.exchange(0) / divisor);

            last_time = std::chrono::steady_clock::now();
            frames = 0;
        }

// -----------------------------------------------------------------------------

        bool any_moved = false;

        if (!skip_update) {
            auto MoveBox = [&](nova::draw::Rectangle& box, nova::VirtualKey left, nova::VirtualKey right, nova::VirtualKey up, nova::VirtualKey down) {
                float speed = 1000.f * delta_time;
                if (app.IsVirtualKeyDown(left))  { box.center_pos.x -= speed; redraw = true; any_moved = true; }
                if (app.IsVirtualKeyDown(right)) { box.center_pos.x += speed; redraw = true; any_moved = true; }
                if (app.IsVirtualKeyDown(up))    { box.center_pos.y -= speed; redraw = true; any_moved = true; }
                if (app.IsVirtualKeyDown(down))  { box.center_pos.y += speed; redraw = true; any_moved = true; }
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
            "Hello Nova Draw 2D API Example",
            Vec2(size.x * 0.25f, size.y * 0.45f),
            *font, font_size);

// -----------------------------------------------------------------------------

        // Record frame

        queue.WaitIdle();

        auto cmd = queue.Begin();

        // Update window size, record primary buffer and present

        // Window borders need to lie on pixel boundaries
        im_draw.bounds.Expand({
            .min = glm::floor(im_draw.bounds.min),
            .max = glm::ceil(im_draw.bounds.max),
        });

        window.SetSize(Vec2U(im_draw.Bounds().Size()), nova::WindowPart::Client);
        queue.Acquire({swapchain});
        auto target = swapchain.Target();

        Vec3 color = { 0.3f, 0.2f, 0.5f };
        f32 alpha = 0.f;
        cmd.ClearColor(target, Vec4(color * alpha, alpha));

        im_draw.Record(cmd, target);

        cmd.Present(swapchain);

        queue.Submit({cmd}, {}).Wait();

        window.SetPosition({ im_draw.Bounds().min.x, im_draw.Bounds().min.y }, nova::WindowPart::Client);
        queue.Present({swapchain}, {});
    }
}
