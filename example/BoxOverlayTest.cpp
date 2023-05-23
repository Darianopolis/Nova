#include <nova/rhi/nova_RHI.hpp>
#include <nova/core/nova_Timer.hpp>

#include <nova/imdraw/nova_ImDraw2D.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <array>
#include <iostream>

using namespace nova::types;

void TryMain()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    // glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
    auto window = glfwCreateWindow(1920, 1200, "next", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) {
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    auto context = nova::Context::Create(true);

    auto surface = context->CreateSurface(glfwGetWin32Window(window));
    auto swapchain = context->CreateSwapchain(surface,
        nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach,
        nova::PresentMode::Fifo);

    auto queue = context->graphics;
    auto commandPool = context->CreateCommandPool();
    auto fence = context->CreateFence();
    auto tracker = context->CreateResourceTracker();

// -----------------------------------------------------------------------------

    auto imDraw = nova::ImDraw2D::Create(context);

// -----------------------------------------------------------------------------

    nova::Image* image;
    nova::ImTextureID imageID;
    {
        i32 w, h, c;
        auto data = stbi_load("assets/textures/statue.jpg", &w, &h, &c, STBI_rgb_alpha);
        NOVA_ON_SCOPE_EXIT(&) { stbi_image_free(data); };

        image = context->CreateImage(
            Vec3(f32(w), f32(h), 0.f),
            nova::ImageUsage::Sampled,
            nova::Format::RGBA8U);

        context->CopyToImage(image, data, w * h * 4);
        context->GenerateMips(image);

        context->transferCmd->Transition(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        context->graphics->Submit({context->transferCmd}, {}, {context->transferFence});
        context->transferFence->Wait();
        context->transferCommandPool->Reset();
        context->transferCmd = context->transferCommandPool->BeginPrimary(context->transferTracker);

        imageID = imDraw->RegisterTexture(image, imDraw->defaultSampler);
    }

// -----------------------------------------------------------------------------

    int count;
    auto mode = glfwGetVideoMode(glfwGetMonitors(&count)[0]);
    int mWidth = mode->width;
    int mHeight = mode->height;

    std::cout << "Monitor size = " << mWidth << ", " << mHeight << '\n';

    auto font = imDraw->LoadFont("assets/fonts/arial.ttf", 20.f);

    nova::ImRoundRect box1 {
        .centerColor = { 1.f, 0.f, 0.f, 0.5f },
        .borderColor = { 0.2f, 0.2f, 0.2f, 1.f },
        .centerPos = { mWidth * 0.25f, mHeight * 0.25f },
        .halfExtent = { 100.f, 200.f },
        .cornerRadius = 15.f,
        .borderWidth = 5.f,

        .texTint = { 1.f, 1.f, 1.f, 1.f },
        .texIndex = imageID,
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
        .texIndex = imageID,
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
        .texIndex = imageID,
        .texCenterPos = { 0.5f, 0.5f },
        .texHalfExtent = { 1.f, 0.5f },
    };

    bool redraw = true;
    bool skipUpdate = false;

    auto lastFrame = std::chrono::steady_clock::now();
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

// -----------------------------------------------------------------------------

        if (!skipUpdate)
        {
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
        else
        {
            redraw = true;
        }

        skipUpdate = false;

        if (!redraw)
        {
            glfwWaitEvents();
            skipUpdate = true;
        }
        redraw = false;

// -----------------------------------------------------------------------------

        imDraw->Reset();

        imDraw->DrawRect(box1);
        imDraw->DrawRect(box2);
        imDraw->DrawRect(box3);

        imDraw->DrawString(
            "C:/Program Files (x86)/Steam/steamapps/common/BeamNG.drive/BeamNG.drive.exe",
            Vec2(mWidth * 0.25f, mHeight * 0.4f),
            font);

// -----------------------------------------------------------------------------

        // Record frame

        fence->Wait();
        commandPool->Reset();

        auto cmd = commandPool->BeginPrimary(tracker);
        cmd->SetViewport({ imDraw->maxBounds.x - imDraw->minBounds.x, imDraw->maxBounds.y - imDraw->minBounds.y }, false);
        cmd->SetBlendState(1, true);
        cmd->SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        // Update window size, record primary buffer and present

        glfwSetWindowSize(window, i32(imDraw->maxBounds.x - imDraw->minBounds.x), i32(imDraw->maxBounds.y - imDraw->minBounds.y));
        glfwSetWindowPos(window, i32(imDraw->minBounds.x), i32(imDraw->minBounds.y));

        queue->Acquire({swapchain}, {fence});

        cmd->BeginRendering({swapchain->image}, {Vec4(0.f)}, true);
        imDraw->Record(cmd);
        cmd->EndRendering();

        cmd->Transition(swapchain->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        queue->Submit({cmd}, {fence}, {fence});
        queue->Present({swapchain}, {fence});
    }
}

int main()
{
    try
    {
        TryMain();
    }
    catch(...) {}
}