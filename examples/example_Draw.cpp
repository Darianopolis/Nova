#include "example_Main.hpp"

#include <nova/core/nova_Timer.hpp>
#include <nova/rhi/nova_RHI.hpp>
#include <nova/ui/nova_ImDraw2D.hpp>

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
    auto commandPool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);
    NOVA_CLEANUP(&) {
        commandPool.Destroy();
        fence.Destroy();
    };

// -----------------------------------------------------------------------------

    nova::ImDraw2D imDraw{context};

// -----------------------------------------------------------------------------

    nova::Texture texture;
    nova::DescriptorHandle texID;
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

        texID = imDraw.RegisterTexture(texture, imDraw.GetDefaultSampler());
    }

// -----------------------------------------------------------------------------

    int count;
    auto mode = glfwGetVideoMode(glfwGetMonitors(&count)[0]);
    int mWidth = mode->width;
    int mHeight = mode->height;

    std::cout << "Monitor size = " << mWidth << ", " << mHeight << '\n';

    auto font = imDraw.LoadFont("assets/fonts/arial.ttf", 20.f);

    nova::ImRoundRect box1 {
        .centerColor = { 1.f, 0.f, 0.f, 0.5f },
        .borderColor = { 0.2f, 0.2f, 0.2f, 1.f },
        .centerPos = { mWidth * 0.25f, mHeight * 0.25f },
        .halfExtent = { 100.f, 200.f },
        .cornerRadius = 15.f,
        .borderWidth = 5.f,

        .texTint = { 1.f, 1.f, 1.f, 1.f },
        .texIndex = texID,
        .texCenterPos = { 0.5f, 0.5f },
        .texHalfExtent = { 0.5f, 1.f },
    };

    nova::ImRoundRect box2 {
        .centerColor = { 0.f, 1.f, 0.f, 0.5f },
        .borderColor = { 0.4f, 0.4f, 0.4f, 1.f },
        .centerPos = { mWidth * 0.5f, mHeight * 0.5f },
        .halfExtent = { 100.f, 100.f },
        .cornerRadius = 15.f,
        .borderWidth = 10.f,

        .texTint = { 1.f, 1.f, 1.f, 1.f },
        .texIndex = texID,
        .texCenterPos = { 0.5f, 0.5f },
        .texHalfExtent = { 0.5f, 0.5f },
    };

    nova::ImRoundRect box3 {
        .centerColor = { 0.f, 0.f, 1.f, 0.5f },
        .borderColor = { 0.6f, 0.6f, 0.6f, 1.f },
        .centerPos = { mWidth * 0.75f, mHeight * 0.75f },
        .halfExtent = { 200.f, 100.f },
        .cornerRadius = 25.f,
        .borderWidth = 10.f,

        .texTint = { 1.f, 1.f, 1.f, 1.f },
        .texIndex = texID,
        .texCenterPos = { 0.5f, 0.5f },
        .texHalfExtent = { 1.f, 0.5f },
    };

    bool redraw = true;
    bool skipUpdate = false;

    auto lastFrame = std::chrono::steady_clock::now();

    NOVA_CLEANUP(&) { fence.Wait(); };
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

// -----------------------------------------------------------------------------

        if (!skipUpdate) {
            auto moveBox = [&](nova::ImRoundRect& box, int left, int right, int up, int down) {
                float speed = 5.f;
                if (glfwGetKey(window, left))  { box.centerPos.x -= speed; redraw = true; }
                if (glfwGetKey(window, right)) { box.centerPos.x += speed; redraw = true; }
                if (glfwGetKey(window, up))    { box.centerPos.y -= speed; redraw = true; }
                if (glfwGetKey(window, down))  { box.centerPos.y += speed; redraw = true; }
            };

            moveBox(box1, GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_W, GLFW_KEY_S);
            moveBox(box2, GLFW_KEY_J, GLFW_KEY_L, GLFW_KEY_I, GLFW_KEY_K);
            moveBox(box3, GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN);
        }
        else {
            redraw = true;
        }

        skipUpdate = false;

        if (!redraw) {
            glfwWaitEvents();
            skipUpdate = true;
        }
        redraw = false;

// -----------------------------------------------------------------------------

        imDraw.Reset();

        imDraw.DrawRect(box1);
        imDraw.DrawRect(box2);
        imDraw.DrawRect(box3);

        imDraw.DrawString(
            "C:/Program Files (x86)/Steam/steamapps/common/BeamNG.drive/BeamNG.drive.exe",
            Vec2(mWidth * 0.25f, mHeight * 0.4f),
            *font);

// -----------------------------------------------------------------------------

        // Record frame

        fence.Wait();
        commandPool.Reset();

        auto cmd = commandPool.Begin();

        // Update window size, record primary buffer and present

        glfwSetWindowSize(window, i32(imDraw.GetBounds().Width()), i32(imDraw.GetBounds().Height()));
        glfwSetWindowPos(window, i32(imDraw.GetBounds().min.x), i32(imDraw.GetBounds().min.y));

        queue.Acquire({swapchain}, {fence});

        cmd.ClearColor(swapchain.GetCurrent(), Vec4(0.f));
        imDraw.Record(cmd, swapchain.GetCurrent());

        cmd.Present(swapchain);

        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});
    }
}
