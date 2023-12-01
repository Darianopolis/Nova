#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Timer.hpp>
#include <nova/rhi/nova_RHI.hpp>
#include <nova/ui/nova_Draw2D.hpp>
#include <nova/window/nova_Window.hpp>

#include <stb_image.h>

NOVA_EXAMPLE(Draw, "draw")
{
    // glfwInit();
    // glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    // auto window = glfwCreateWindow(1920, 1200, "Nova - Draw", nullptr, nullptr);
    // NOVA_DEFER(&) {
    //     glfwDestroyWindow(window);
    //     glfwTerminate();
    // };

    // HWND hwnd = glfwGetWin32Window(window);
    // SetWindowLongW(hwnd, GWL_EXSTYLE, GetWindowLongW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);

    // // TODO: Chroma key is an ugly hack, use nchittest to do analytical transparency
    // //   Or, do full screeen pass to filter out unintentional chroma key matches and
    // //   apply chroma key based on alpha.
    // SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app, {
        .title = "Nova - Draw",
        .size = { 1920, 1080 },
    });

    auto context = nova::Context::Create({
        .debug = true,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    auto swapchain = nova::Swapchain::Create(context, window.GetNativeHandle(),
        nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach,
        nova::PresentMode::Fifo);
    NOVA_DEFER(&) { swapchain.Destroy(); };

    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto command_pool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);
    NOVA_DEFER(&) {
        command_pool.Destroy();
        fence.Destroy();
    };

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

    int count;
    // auto mode = glfwGetVideoMode(glfwGetMonitors(&count)[0]);
    // int width = mode->width;
    // int height = mode->height;
    auto size = app.GetPrimaryDisplay().GetSize();

    std::cout << "Monitor size = " << size.x << ", " << size.y << '\n';

    auto font = im_draw.LoadFont("assets/fonts/arial.ttf", 20.f);

    nova::draw::Rectangle box1 {
        .center_color = { 1.f, 0.f, 0.f, 0.5f },
        .border_color = { 0.2f, 0.2f, 0.2f, 1.f },
        .center_pos = { size.x * 0.25f, size.y * 0.25f },
        .half_extent = { 100.f, 200.f },
        .corner_radius = 15.f,
        .border_width = 5.f,

        .tex_tint = { 1.f, 1.f, 1.f, 1.f },
        .tex_idx = image.GetDescriptor(),
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
        .tex_idx = image.GetDescriptor(),
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
        .tex_idx = image.GetDescriptor(),
        .tex_center_pos = { 0.5f, 0.5f },
        .tex_half_extent = { 1.f, 0.5f },
    };

    bool redraw = true;
    bool skip_update = false;

    auto last_frame = std::chrono::steady_clock::now();

    NOVA_DEFER(&) { fence.Wait(); };
    while (app.IsRunning()) {
        app.PollEvents();
        if (!app.IsRunning()) {
            break;
        }

// -----------------------------------------------------------------------------

        if (!skip_update) {
            auto MoveBox = [&](nova::draw::Rectangle& box, nova::Button left, nova::Button right, nova::Button up, nova::Button down) {
                float speed = 5.f;
                if (app.IsButtonDown(left))  { box.center_pos.x -= speed; redraw = true; }
                if (app.IsButtonDown(right)) { box.center_pos.x += speed; redraw = true; }
                if (app.IsButtonDown(up))    { box.center_pos.y -= speed; redraw = true; }
                if (app.IsButtonDown(down))  { box.center_pos.y += speed; redraw = true; }
            };

            // MoveBox(box1, GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_W, GLFW_KEY_S);
            // MoveBox(box2, GLFW_KEY_J, GLFW_KEY_L, GLFW_KEY_I, GLFW_KEY_K);
            MoveBox(box3, nova::Button::Left, nova::Button::Right, nova::Button::Up, nova::Button::Down);
        }
        else {
            redraw = true;
        }

        skip_update = false;

        if (!redraw) {
            app.WaitEvents();
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

        fence.Wait();
        command_pool.Reset();

        auto cmd = command_pool.Begin();

        // Update window size, record primary buffer and present

        window.SetSize({ im_draw.GetBounds().Width(), im_draw.GetBounds().Height() }, nova::WindowPart::Client);
        window.SetPosition({ im_draw.GetBounds().min.x, im_draw.GetBounds().min.y }, nova::WindowPart::Client);

        queue.Acquire({swapchain}, {fence});

        cmd.ClearColor(swapchain.GetCurrent(), Vec4(0.f));
        im_draw.Record(cmd, swapchain.GetCurrent());

        cmd.Present(swapchain);

        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});
    }
}
