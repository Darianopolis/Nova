#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/nova_RHI_Handle.hpp>

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

using namespace nova::types;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1200, "Hello Nova RT Triangle", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) {
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    NOVA_LOG("Size = {}", sizeof(std::mutex));

    auto ctx = std::make_unique<nova::VulkanContext>(nova::ContextConfig {
        .debug = true,
    });
    auto context = nova::HContext(ctx.get());

    auto swapchain = context.CreateSwapchain(glfwGetWin32Window(window),
        nova::TextureUsage::ColorAttach
        | nova::TextureUsage::TransferDst,
        nova::PresentMode::Fifo);

    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto cmdPool = context.CreateCommandPool(queue);
    auto fence = context.CreateFence();
    auto state = context.CreateCommandState();

    // Vertex data

    struct Vertex
    {
        Vec3 position;
        Vec3 color;
    };

    auto vertices = context.CreateBuffer(3 * sizeof(Vertex),
        nova::BufferUsage::Storage,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    // vertices.Set<Vertex>({ {{-0.6f, 0.6f, 0.f}, {1.f,0.f,0.f}}, {{0.6f, 0.6f, 0.f},{0.f,1.f,0.f}}, {{0.f, -0.6f, 0.f},{0.f,0.f,1.f}} });
    /*
    {1.0f,1.0f,0.0f,    1.0f,1.0f,    0.0f,0.0f,0.0f,0.0f},
        {1.0f,-1.0f,0.0f,    1.0f,0.0f,    0.0f,0.0f,0.0f,0.0f},
        {-1.0f,-1.0f,0.0f,    0.0f,0.0f,    0.0f,0.0f,0.0f,0.0f},
        {-1.0f,1.0f,0.0f,    0.0f,1.0f,    0.0f,0.0f,0.0f,0.0f}*/
    vertices.Set<Vertex>({
        {{1.0f,1.0f,0.0f}, {1.0f,1.0f,0.5f}},
        {{1.0f,-1.0f,0.0f}, {1.0f,0.0f,0.5f}},
        {{-1.0f,-1.0f,0.0f}, {.0f,0.0f,0.5f}},
        {{-1.0f,1.0f,0.0f}, {0.0f,1.0f,0.5f}},
    });

    auto indices = context.CreateBuffer(6 * 4,
        nova::BufferUsage::Index,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    indices.Set<u32>({0,1,3,1,2,3});

    // Pipeline

    auto descLayout = context.CreateDescriptorSetLayout({
        nova::binding::UniformBuffer("ubo", {{"pos", nova::ShaderVarType::Vec3}}),
    });
    auto pcRange = nova::PushConstantRange("pc", {{"vertexVA", nova::ShaderVarType::U64},});
    auto pipelineLayout = context.CreatePipelineLayout({pcRange}, {descLayout}, nova::BindPoint::Graphics);

    // UBO descriptor set

    auto ubo = context.CreateBuffer(sizeof(Vec3),
        nova::BufferUsage::Uniform,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    ubo.Set<Vec3>({{0.f, 0.f, 0.f}});

    auto set = descLayout.Allocate();
    set.WriteUniformBuffer(0, ubo);

    // Create vertex shader

    auto vertexShader = context.CreateShader(nova::ShaderStage::Vertex, {
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

    auto fragmentShader = context.CreateShader(nova::ShaderStage::Fragment, {
        nova::shader::Input("inColor", nova::ShaderVarType::Vec3),
        nova::shader::Output("fragColor", nova::ShaderVarType::Vec4),
        nova::shader::Kernel("fragColor = vec4(inColor, 1);"),
    });

    // Draw

    NOVA_ON_SCOPE_EXIT(&) { fence.Wait(); };
    while (!glfwWindowShouldClose(window))
    {
        fence.Wait();
        queue.Acquire({swapchain}, {fence});

        cmdPool.Reset();
        auto cmd = cmdPool.Begin(state);

        cmd.BeginRendering({{}, swapchain.GetExtent()}, {swapchain.GetCurrent()});
        cmd.ClearColor(0, Vec4(Vec3(0.1f), 1.f), swapchain.GetExtent());
        cmd.SetGraphicsState(pipelineLayout, {vertexShader, fragmentShader}, {});
        cmd.PushConstants(pipelineLayout, 0, sizeof(u64), nova::Temp(vertices.GetAddress()));
        cmd.BindDescriptorSets(pipelineLayout, 0, {set});
        cmd.BindIndexBuffer(indices, nova::IndexType::U32);
        // cmd.Draw(3, 1, 0, 0)
        cmd.DrawIndexed(6, 1, 0, 0, 0);
        cmd.EndRendering();

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        glfwWaitEvents();
    }
}