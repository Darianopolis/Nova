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

    auto ctx = std::make_unique<nova::VulkanContext>(nova::ContextConfig {
        .debug = true,
    });
    auto context = ctx.get();

    auto swapchain = nova::HSwapchain(context, glfwGetWin32Window(window),
        nova::TextureUsage::ColorAttach
        | nova::TextureUsage::TransferDst,
        nova::PresentMode::Fifo);

    auto queue = nova::HQueue(context, nova::QueueFlags::Graphics);
    auto cmdPool = nova::HCommandPool(context, queue);
    auto fence = nova::HFence(context);
    auto state = nova::HCommandState(context);

    // Vertex data

    struct Vertex
    {
        Vec3 position;
        Vec3 color;
    };

    auto vertices = nova::HBuffer(context, 3 * sizeof(Vertex),
        nova::BufferUsage::Storage,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    vertices.Set<Vertex>({ {{-0.6f, 0.6f, 0.f}, {1.f,0.f,0.f}}, {{0.6f, 0.6f, 0.f},{0.f,1.f,0.f}}, {{0.f, -0.6f, 0.f},{0.f,0.f,1.f}} });

    // Pipeline

    auto descLayout = nova::HDescriptorSetLayout(context, {
        nova::binding::UniformBuffer("ubo", {{"pos", nova::ShaderVarType::Vec3}}),
    });
    auto pcRange = nova::PushConstantRange("pc", {{"vertexVA", nova::ShaderVarType::U64},});
    auto pipelineLayout = nova::HPipelineLayout(context, {pcRange}, {descLayout}, nova::BindPoint::Graphics);

    // UBO descriptor set

    auto ubo = nova::HBuffer(context, sizeof(Vec3),
        nova::BufferUsage::Uniform,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    ubo.Set<Vec3>({{0.f, 0.25f, 0.f}});

    auto set = descLayout.Allocate();
    set.WriteUniformBuffer(0, ubo);

    // Create vertex shader

    auto vertexShader = nova::HShader(context, nova::ShaderStage::Vertex, {
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

    auto fragmentShader = nova::HShader(context, nova::ShaderStage::Fragment, {
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

        cmd.BeginRendering({swapchain.GetCurrent()});
        cmd.ClearColor(0, Vec4(Vec3(0.1f), 1.f), swapchain.GetExtent());
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