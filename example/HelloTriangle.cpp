#include <nova/rhi/nova_RHI.hpp>

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

    auto context = +nova::Context({
        .debug = true,
    });

    auto surface = +nova::Surface(context, glfwGetWin32Window(window));
    auto swapchain = +nova::Swapchain(context, surface,
        nova::TextureUsage::ColorAttach
        | nova::TextureUsage::TransferDst,
        nova::PresentMode::Fifo);

    auto queue = context.GetQueue(nova::QueueFlags::Graphics);
    auto cmdPool = +nova::CommandPool(context, queue);
    auto fence = +nova::Fence(context);
    auto state = +nova::CommandState(context);

    // Pipeline

    struct Vertex
    {
        Vec3 position;
        Vec3 color;
    };

    auto vertices = +nova::Buffer(context, 3 * sizeof(Vertex),
        nova::BufferUsage::Storage,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);
    vertices.Set<Vertex>({ {{-0.6f, 0.6f, 0.f}, {1.f,0.f,0.f}}, {{0.6f, 0.6f, 0.f},{0.f,1.f,0.f}}, {{0.f, -0.6f, 0.f},{0.f,0.f,1.f}} });

    auto pipelineLayout = +nova::PipelineLayout(context, {{nova::ShaderStage::Vertex, sizeof(u64)}}, {}, nova::BindPoint::Graphics);

    auto vertexShader = +nova::Shader(context, nova::ShaderStage::Vertex, {
        nova::shader::Structure("Vertex", {
            {"position", nova::ShaderVarType::Vec3},
            {"color", nova::ShaderVarType::Vec3},
        }),
        nova::shader::BufferReference("Vertex"),
        nova::shader::PushConstants("pc", {
            {"vertexVA", nova::ShaderVarType::U64},
        }),
        nova::shader::Output("color", nova::ShaderVarType::Vec3),
        nova::shader::Kernel(R"(
            Vertex v = Vertex_BR(pc.vertexVA)[gl_VertexIndex];
            color = v.color;
            gl_Position = vec4(v.position, 1);
        )"),
    });

    auto fragmentShader = +nova::Shader(context, nova::ShaderStage::Fragment, {
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
        cmd.PushConstants(pipelineLayout, nova::ShaderStage::Vertex, 0, sizeof(u64), nova::Temp(vertices.GetAddress()));
        cmd.Draw(3, 1, 0, 0);
        cmd.EndRendering();

        cmd.Present(swapchain.GetCurrent());
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        glfwWaitEvents();
    }
}