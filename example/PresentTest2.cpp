#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/nova_RHI_Handle.hpp>

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

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
    HWND hwnd = glfwGetWin32Window(window);

    NOVA_LOGEXPR(ctx);
    auto swapchain = nova::HSwapchain(ctx.get(), hwnd,
        nova::TextureUsage::ColorAttach | nova::TextureUsage::Storage,
        nova::PresentMode::Mailbox);
    NOVA_ON_SCOPE_EXIT(&) {
        ctx->WaitIdle();
        swapchain.Destroy();
    };

    auto queue = nova::HQueue(ctx.get(), nova::QueueFlags::Graphics);
    auto fence = nova::HFence(ctx.get());
    auto cmdState = nova::HCommandState(ctx.get());
    auto cmdPool = nova::HCommandPool(ctx.get(), queue);

    struct Vertex
    {
        Vec3 position;
        Vec3 color;
    };

    auto vertices = nova::HBuffer(ctx.get(), 3 * sizeof(Vertex),
        nova::BufferUsage::Storage,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    vertices.Set<Vertex>({
        {{-0.6f, 0.6f, 0.f}, {1.f, 0.f, 0.f}},
        {{ 0.6f, 0.6f, 0.f}, {0.f, 1.f, 0.f}},
        {{ 0.f, -0.6f, 0.f}, {0.f, 0.f, 1.f}}
    });

    auto descLayout = nova::HDescriptorSetLayout(ctx.get(), {
        nova::binding::UniformBuffer("ubo", {{"pos", nova::ShaderVarType::Vec3}}),
    });

    auto pipelineLayout = nova::HPipelineLayout(ctx.get(),
        {{"pc", {{"vertexVA", nova::ShaderVarType::U64}}}},
        {descLayout}, nova::BindPoint::Graphics);

    auto ubo = nova::HBuffer(ctx.get(), sizeof(Vec3),
        nova::BufferUsage::Uniform,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    ubo.Set<Vec3>({{0.f, 0.25f, 0.f}});

    auto set = descLayout.Allocate();
    set.WriteUniformBuffer(0, ubo);

    auto vertexShader = nova::HShader(ctx.get(), nova::ShaderStage::Vertex, {
        nova::shader::Structure("Vertex", {
            {"position", nova::ShaderVarType::Vec3},
            {"color", nova::ShaderVarType::Vec3},
        }),
        nova::shader::Layout(pipelineLayout),

        nova::shader::Output("color", nova::ShaderVarType::Vec3),
        nova::shader::Kernel(R"(
            Vertex v = Vertex_BR(pc.vertexVA)[gl_VertexIndex];
            color = v.color;
            gl_Position = vec4(v.position + ubo.pos, 1);
        )"),
    });

    auto fragmentShader = nova::HShader(ctx.get(), nova::ShaderStage::Fragment, {
        nova::shader::Input("inColor", nova::ShaderVarType::Vec3),
        nova::shader::Output("fragColor", nova::ShaderVarType::Vec4),
        nova::shader::Kernel("fragColor = vec4(inColor, 1);"),
    });

    nova::HTexture texture;
    {
        i32 w, h;
        auto data = stbi_load("assets/textures/statue.jpg", &w, &h, nullptr, STBI_rgb_alpha);
        NOVA_ON_SCOPE_EXIT(&) { stbi_image_free(data); };

        texture = nova::HTexture(ctx.get(), { u32(w), u32(h), 0 },
            nova::TextureUsage::Sampled,
            nova::Format::RGBA8U);

        usz size = w * h * 4;
        auto staging = nova::HBuffer(ctx.get(), size, nova::BufferUsage::TransferSrc, nova::BufferFlags::Mapped);
        NOVA_ON_SCOPE_EXIT(&) { staging.Destroy(); };
        std::memcpy(staging.GetMapped(), data, size);

        auto cmd = cmdPool.Begin(cmdState);
        cmd.CopyToTexture(texture, staging);
        cmd.GenerateMips(texture);

        queue.Submit({cmd}, {}, {fence});
        fence.Wait();
    }

    NOVA_ON_SCOPE_EXIT(&) { ctx->WaitIdle(); };
    while (!glfwWindowShouldClose(window))
    {
        fence.Wait();
        queue.Acquire({swapchain}, {fence});
        auto target = swapchain.GetCurrent();

        cmdPool.Reset();
        auto cmd = cmdPool.Begin(cmdState);

        cmd.Clear(target, Vec4(33 / 255.f, 81 / 255.f, 68 / 255.f, 1.f));

        cmd.BlitImage(target, texture, nova::Filter::Linear);

        cmd.BeginRendering({target});
        cmd.SetGraphicsState(pipelineLayout, {vertexShader, fragmentShader}, {});
        cmd.PushConstants(pipelineLayout, 0, sizeof(u64), nova::Temp(vertices.GetAddress()));
        cmd.BindDescriptorSets(pipelineLayout, 0, {set});
        cmd.Draw(3, 1, 0, 0);
        cmd.EndRendering();

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

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