#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/nova_RHI_Handle.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <stb_image.h>

using namespace nova::types;

void TryMain()
{
    auto ctx = nova::Context::Create({
        .backend = nova::Backend::Vulkan,
        .debug = true,
    });

    glfwInit();
    NOVA_ON_SCOPE_EXIT(&) { glfwTerminate(); };
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto window = glfwCreateWindow(1920, 1200, "Present Test", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) { glfwDestroyWindow(window); };
    HWND hwnd = glfwGetWin32Window(window);

    NOVA_LOGEXPR(ctx.Raw());
    auto swapchain = ctx->Swapchain_Create(hwnd,
        nova::TextureUsage::ColorAttach | nova::TextureUsage::Storage,
        nova::PresentMode::Mailbox);
    NOVA_ON_SCOPE_EXIT(&) {
        ctx->WaitIdle();
        ctx->Swapchain_Destroy(swapchain);
    };

    auto queue = ctx->Queue_Get(nova::QueueFlags::Graphics, 0);
    auto fence = ctx->Fence_Create();
    auto cmdState = ctx->Commands_CreateState();
    auto cmdPool = ctx->Commands_CreatePool(queue);

    struct Vertex
    {
        Vec3 position;
        Vec3 color;
    };

    auto vertices = ctx->Buffer_Create(3 * sizeof(Vertex),
        nova::BufferUsage::Storage,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    ctx->Buffer_Set<Vertex>(vertices, {
        {{-0.6f, 0.6f, 0.f}, {1.f, 0.f, 0.f}},
        {{ 0.6f, 0.6f, 0.f}, {0.f, 1.f, 0.f}},
        {{ 0.f, -0.6f, 0.f}, {0.f, 0.f, 1.f}}
    });

    auto descLayout = ctx->Descriptors_CreateSetLayout({
        nova::binding::UniformBuffer("ubo", {{"pos", nova::ShaderVarType::Vec3}}),
    });

    auto pipelineLayout = ctx->Pipelines_CreateLayout(
        {{"pc", {{"vertexVA", nova::ShaderVarType::U64}}}},
        {descLayout}, nova::BindPoint::Graphics);

    auto ubo = ctx->Buffer_Create(sizeof(Vec3),
        nova::BufferUsage::Uniform,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    ctx->Buffer_Set<Vec3>(ubo, {{0.f, 0.25f, 0.f}});

    auto set = ctx->Descriptors_AllocateSet(descLayout);
    ctx->Descriptors_WriteUniformBuffer(set, 0, ubo);

    auto vertexShader = ctx->Shader_Create(nova::ShaderStage::Vertex, {
        nova::shader::Structure("Vertex", {
            {"position", nova::ShaderVarType::Vec3},
            {"color", nova::ShaderVarType::Vec3},
        }),
        nova::shader::Layout(pipelineLayout),

        nova::shader::Output("color", nova::ShaderVarType::Vec3),
        nova::shader::Kernel(R"glsl(
            Vertex v = Vertex_BR(pc.vertexVA)[gl_VertexIndex];
            color = v.color;
            gl_Position = vec4(v.position + ubo.pos, 1);
        )glsl"),
    });

    auto fragmentShader = ctx->Shader_Create(nova::ShaderStage::Fragment, {
        nova::shader::Input("inColor", nova::ShaderVarType::Vec3),
        nova::shader::Output("fragColor", nova::ShaderVarType::Vec4),
        nova::shader::Kernel("fragColor = vec4(inColor, 1);"),
    });

    nova::Texture texture;
    {
        i32 w, h;
        auto data = stbi_load("assets/textures/statue.jpg", &w, &h, nullptr, STBI_rgb_alpha);
        NOVA_ON_SCOPE_EXIT(&) { stbi_image_free(data); };

        texture = ctx->Texture_Create({ u32(w), u32(h), 0 },
            nova::TextureUsage::Sampled,
            nova::Format::RGBA8_UNorm);

        usz size = w * h * 4;
        auto staging = ctx->Buffer_Create(size, nova::BufferUsage::TransferSrc, nova::BufferFlags::Mapped);
        NOVA_ON_SCOPE_EXIT(&) { ctx->Buffer_Destroy(staging); };
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

        ctx->Cmd_BeginRendering(cmd, {{}, ctx->Swapchain_GetExtent(swapchain)}, {target});
        ctx->Cmd_SetGraphicsState(cmd, pipelineLayout, {vertexShader, fragmentShader}, {});
        ctx->Cmd_PushConstants(cmd, pipelineLayout, 0, sizeof(u64), nova::Temp(ctx->Buffer_GetAddress(vertices)));
        ctx->Cmd_BindDescriptorSets(cmd, pipelineLayout, 0, {set});
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