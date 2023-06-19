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

    auto pipelineLayout = +nova::PipelineLayout(context, {}, {}, nova::BindPoint::Graphics);

    auto vertexShader = +nova::Shader(context,
        nova::ShaderStage::Vertex,
        "vertex",
        nova::Shader::GenerateShader(
            {},
            {},
            {},
            {R"(
const vec2 positions[3] = vec2[] (vec2(-0.6, 0.6), vec2(0.6, 0.6), vec2(0, -0.6));
const vec3    colors[3] = vec3[] (vec3(1, 0, 0),   vec3(0, 1, 0),  vec3(0, 0, 1));

layout(location = 0) out vec3 color;

void main()
{
    color = colors[gl_VertexIndex];
    gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
}
            )"}));

    auto fragmentShader = +nova::Shader(context,
        nova::ShaderStage::Fragment,
        "fragment",
        nova::Shader::GenerateShader(
            {},
            {},
            {},
            {R"(
layout(location = 0) in  vec3   inColor;
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(inColor, 1);
}
            )"}));

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
        cmd.Draw(3, 1, 0, 0);
        cmd.EndRendering();

        cmd.Present(swapchain.GetCurrent());
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        glfwWaitEvents();
    }
}