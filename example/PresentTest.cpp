#include <nova/rhi/nova_RHI.hpp>

#include <nova/rhi/vulkan/nova_VulkanContext.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <stb_image.h>

using namespace nova::types;

void TryMain()
{
    auto ctx = std::make_unique<nova::VulkanContext>(nova::ContextConfig {
        .debug = true,
    });

    glfwInit();
    NOVA_ON_SCOPE_EXIT(&) { glfwTerminate(); };
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto window = glfwCreateWindow(1920, 1200, "Present Test", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) { glfwDestroyWindow(window); };

    auto swapchain = ctx->Swapchain_Create(glfwGetWin32Window(window),
        nova::TextureUsage::ColorAttach | nova::TextureUsage::Storage,
        nova::PresentMode::Mailbox);
    NOVA_ON_SCOPE_EXIT(&) {
        ctx->WaitIdle();
        ctx->Destroy(swapchain);
    };

    auto queue = ctx->Queue_Get(nova::QueueFlags::Graphics);
    auto fence = ctx->Fence_Create();
    auto cmdState = ctx->Commands_CreateState();
    auto cmdPool = ctx->Commands_CreatePool(queue);

    auto vertexShader = ctx->Shader_Create(nova::ShaderStage::Vertex,
        "vertex",
        R"(
#version 460

const vec2 Positions[3] = vec2[] (vec2(-0.6, 0.6), vec2(0.6, 0.6), vec2(0, -0.6));
const vec3 Colors[3]    = vec3[] (vec3(1, 0, 0),   vec3(0, 1, 0),  vec3(0, 0, 1));

layout(location = 0) out vec3 outColor;

void main()
{
    outColor = Colors[gl_VertexIndex];
    gl_Position = vec4(Positions[gl_VertexIndex], 0, 1);
}
        )");

    auto fragmentShader = ctx->Shader_Create(nova::ShaderStage::Fragment,
        "fragment",
        R"(
#version 460

layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(inColor, 1);
}
        )");

    auto pipelineLayout = ctx->Pipelines_CreateLayout();

    nova::Texture texture;
    {
        i32 w, h;
        auto data = stbi_load("assets/textures/statue.jpg", &w, &h, nullptr, STBI_rgb_alpha);
        NOVA_ON_SCOPE_EXIT(&) { stbi_image_free(data); };

        texture = ctx->Texture_Create({ u32(w), u32(h), 0 },
            nova::TextureUsage::Sampled,
            nova::Format::RGBA8U);

        usz size = w * h * 4;
        auto staging = ctx->Buffer_Create(size, nova::BufferUsage::TransferSrc, nova::BufferFlags::Mapped);
        NOVA_ON_SCOPE_EXIT(&) { ctx->Destroy(staging); };
        std::memcpy(ctx->Buffer_GetMapped(staging), data, size);

        auto cmd = ctx->Commands_Begin(cmdPool, cmdState);
        ctx->Cmd_CopyToTexture(cmd, texture, staging);
        ctx->Cmd_GenerateMips(cmd, texture);

        ctx->Queue_Submit(queue, {cmd}, {}, {fence});
        ctx->Fence_Wait(fence);
    }

    while (!glfwWindowShouldClose(window))
    {
        ctx->Fence_Wait(fence);
        ctx->Queue_Acquire(queue, {swapchain}, {fence});
        auto target = ctx->Swapchain_GetCurrent(swapchain);

        ctx->Commands_Reset(cmdPool);
        auto cmd = ctx->Commands_Begin(cmdPool, cmdState);

        ctx->Cmd_Clear(cmd, target, Vec4(33 / 255.f, 81 / 255.f, 68 / 255.f, 1.f));

        ctx->Cmd_BlitImage(cmd, target, texture, nova::Filter::Linear);

        ctx->Cmd_BeginRendering(cmd, {target});
        ctx->Cmd_SetGraphicsState(cmd, pipelineLayout, {vertexShader, fragmentShader}, {});
        ctx->Cmd_Draw(cmd, 3, 1, 0, 0);
        ctx->Cmd_EndRendering(cmd);

        ctx->Cmd_Present(cmd, swapchain);
        ctx->Queue_Submit(queue, {cmd}, {fence}, {fence});
        ctx->Queue_Present(queue, {swapchain}, {fence});

        glfwWaitEvents();
    }
}

int main()
{
    try
    {
        TryMain();
    }
    catch(...)
    {
        NOVA_LOG("Exiting on exception...");
    }
}