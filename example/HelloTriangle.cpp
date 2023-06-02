#include <nova/rhi/nova_RHI.hpp>

using namespace nova;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1200, "Hello Nova RT Triangle", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) {
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    auto context = Context::Create({
        .debug = false,
    });
    NOVA_ON_SCOPE_EXIT(&) { Context::Destroy(context); };

    nova::Surface surface(context, glfwGetWin32Window(window));
    nova::Swapchain swapchain(context, &surface,
        TextureUsage::ColorAttach
        | TextureUsage::TransferDst,
        PresentMode::Fifo);

    auto queue = context->graphics;
    auto cmdPool = context->CreateCommandPool();
    auto fence = context->CreateFence();
    auto tracker = context->CreateResourceTracker();
    NOVA_ON_SCOPE_EXIT(&) {
        context->DestroyCommandPool(cmdPool);
        context->DestroyFence(fence);
        context->DestroyResourceTracker(tracker);
    };

    // Pipeline

    auto pipelineLayout = context->CreatePipelineLayout({}, {}, nova::BindPoint::RayTracing);
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyPipelineLayout(pipelineLayout); };

    Shader vertexShader(context,
        ShaderStage::Vertex, ShaderStage::Fragment,
        "vertex",
        R"(
#version 460

const vec2 positions[3] = vec2[] (vec2(-0.6, 0.6), vec2(0.6, 0.6), vec2(0, -0.6));
const vec3    colors[3] = vec3[] (vec3(1, 0, 0),   vec3(0, 1, 0),  vec3(0, 0, 1));

layout(location = 0) out vec3 color;

void main()
{
    color = colors[gl_VertexIndex];
    gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
}
        )",
        pipelineLayout);

    Shader fragmentShader(context,
        ShaderStage::Fragment, {},
        "fragment",
        R"(
#version 460

layout(location = 0) in  vec3   inColor;
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(inColor, 1);
}
        )",
        pipelineLayout);

    // Draw

    NOVA_ON_SCOPE_EXIT(&) { fence->Wait(); };
    while (!glfwWindowShouldClose(window))
    {
        fence->Wait();
        queue->Acquire({&swapchain}, {fence});

        cmdPool->Reset();
        auto cmd = cmdPool->BeginPrimary(tracker);

        cmd->SetViewport(Vec2U(swapchain.current->extent), false);
        cmd->SetBlendState(1, false);
        cmd->SetDepthState(false, false, nova::CompareOp::Greater);
        cmd->SetTopology(nova::Topology::Triangles);
        cmd->SetCullState(nova::CullMode::None, nova::FrontFace::CounterClockwise);

        cmd->BindShaders({&vertexShader, &fragmentShader});
        cmd->BeginRendering({swapchain.current});
        cmd->ClearColor(0, Vec4(Vec3(0.1f), 1.f), Vec2U(swapchain.current->extent));
        cmd->Draw(3, 1, 0, 0);
        cmd->EndRendering();

        cmd->Present(swapchain.current);
        queue->Submit({cmd}, {fence}, {fence});
        queue->Present({&swapchain}, {fence});

        glfwWaitEvents();
    }
}