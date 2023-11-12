#include "example_Main.hpp"

#include <nova/core/nova_Timer.hpp>
#include <nova/rhi/nova_RHI.hpp>
#include <nova/ui/nova_ImDraw2D.hpp>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <stb_image.h>

NOVA_EXAMPLE(Draw, "draw")
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    auto window = glfwCreateWindow(1920, 1200, "Nova - Draw", nullptr, nullptr);
    NOVA_CLEANUP(&) {
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    HWND hwnd = glfwGetWin32Window(window);
    SetWindowLongW(hwnd, GWL_EXSTYLE, GetWindowLongW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);

    // TODO: Chroma key is an ugly hack, use nchittest to do analytical transparency
    //   Or, do full screeen pass to filter out unintentional chroma key matches and
    //   apply chroma key based on alpha.
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    auto context = nova::Context::Create({
        .debug = true,
    });
    NOVA_CLEANUP(&) { context.Destroy(); };

    auto swapchain = nova::Swapchain::Create(context, hwnd,
        nova::TextureUsage::TransferDst
        | nova::TextureUsage::ColorAttach,
        nova::PresentMode::Fifo);
    NOVA_CLEANUP(&) { swapchain.Destroy(); };

    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto command_pool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);
    NOVA_CLEANUP(&) {
        command_pool.Destroy();
        fence.Destroy();
    };

// -----------------------------------------------------------------------------

    nova::ImDraw2D im_draw{ context };

// -----------------------------------------------------------------------------

    nova::Texture texture;
    {
        i32 w, h, c;
        auto data = stbi_load("assets/textures/statue.jpg", &w, &h, &c, STBI_rgb_alpha);
        NOVA_CLEANUP(&) { stbi_image_free(data); };

        texture = nova::Texture::Create(context,
            Vec3U(u32(w), u32(h), 0),
            nova::TextureUsage::Sampled,
            nova::Format::RGBA8_UNorm);

        texture.Set({}, {u32(w), u32(h), 1}, data);
        texture.Transition(nova::TextureLayout::Sampled);
    }

// -----------------------------------------------------------------------------

    int count;
    auto mode = glfwGetVideoMode(glfwGetMonitors(&count)[0]);
    int width = mode->width;
    int height = mode->height;

    std::cout << "Monitor size = " << width << ", " << height << '\n';

    auto font = im_draw.LoadFont("assets/fonts/arial.ttf", 20.f);

    nova::ImRoundRect box1 {
        .center_color = { 1.f, 0.f, 0.f, 0.5f },
        .border_color = { 0.2f, 0.2f, 0.2f, 1.f },
        .center_pos = { width * 0.25f, height * 0.25f },
        .half_extent = { 100.f, 200.f },
        .corner_radius = 15.f,
        .border_width = 5.f,

        .tex_tint = { 1.f, 1.f, 1.f, 1.f },
        .tex_idx = texture.GetDescriptor(),
        .tex_center_pos = { 0.5f, 0.5f },
        .tex_half_extent = { 0.5f, 1.f },
    };

    nova::ImRoundRect box2 {
        .center_color = { 0.f, 1.f, 0.f, 0.5f },
        .border_color = { 0.4f, 0.4f, 0.4f, 1.f },
        .center_pos = { width * 0.5f, height * 0.5f },
        .half_extent = { 100.f, 100.f },
        .corner_radius = 15.f,
        .border_width = 10.f,

        .tex_tint = { 1.f, 1.f, 1.f, 1.f },
        .tex_idx = texture.GetDescriptor(),
        .tex_center_pos = { 0.5f, 0.5f },
        .tex_half_extent = { 0.5f, 0.5f },
    };

    nova::ImRoundRect box3 {
        .center_color = { 0.f, 0.f, 1.f, 0.5f },
        .border_color = { 0.6f, 0.6f, 0.6f, 1.f },
        .center_pos = { width * 0.75f, height * 0.75f },
        .half_extent = { 200.f, 100.f },
        .corner_radius = 25.f,
        .border_width = 10.f,

        .tex_tint = { 1.f, 1.f, 1.f, 1.f },
        .tex_idx = texture.GetDescriptor(),
        .tex_center_pos = { 0.5f, 0.5f },
        .tex_half_extent = { 1.f, 0.5f },
    };

    bool redraw = true;
    bool skip_update = false;

    auto last_frame = std::chrono::steady_clock::now();

    NOVA_CLEANUP(&) { fence.Wait(); };
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

// -----------------------------------------------------------------------------

        if (!skip_update) {
            auto MoveBox = [&](nova::ImRoundRect& box, int left, int right, int up, int down) {
                float speed = 5.f;
                if (glfwGetKey(window, left))  { box.center_pos.x -= speed; redraw = true; }
                if (glfwGetKey(window, right)) { box.center_pos.x += speed; redraw = true; }
                if (glfwGetKey(window, up))    { box.center_pos.y -= speed; redraw = true; }
                if (glfwGetKey(window, down))  { box.center_pos.y += speed; redraw = true; }
            };

            MoveBox(box1, GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_W, GLFW_KEY_S);
            MoveBox(box2, GLFW_KEY_J, GLFW_KEY_L, GLFW_KEY_I, GLFW_KEY_K);
            MoveBox(box3, GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN);
        }
        else {
            redraw = true;
        }

        skip_update = false;

        if (!redraw) {
            glfwWaitEvents();
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
            Vec2(width * 0.25f, height * 0.4f),
            *font);

// -----------------------------------------------------------------------------

        // Record frame

        fence.Wait();
        command_pool.Reset();

        auto cmd = command_pool.Begin();

        // Update window size, record primary buffer and present

        glfwSetWindowSize(window, i32(im_draw.GetBounds().Width()), i32(im_draw.GetBounds().Height()));
        glfwSetWindowPos(window, i32(im_draw.GetBounds().min.x), i32(im_draw.GetBounds().min.y));

        queue.Acquire({swapchain}, {fence});

        cmd.ClearColor(swapchain.GetCurrent(), Vec4(0.f));
        im_draw.Record(cmd, swapchain.GetCurrent());

        cmd.Present(swapchain);

        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});
    }
}
