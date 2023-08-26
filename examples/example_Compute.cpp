#include "example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

void example_Compute()
{

// -----------------------------------------------------------------------------
//                             GLFW Initialization
// -----------------------------------------------------------------------------

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1200, "Nova - Compute", nullptr, nullptr);
    NOVA_CLEANUP(&) {
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    i32 width, height;
    glfwGetFramebufferSize(window, &width, &height);

// -----------------------------------------------------------------------------
//                             Nova Initialization
// -----------------------------------------------------------------------------

    auto context = nova::Context::Create({
        .debug = false,
    });
    NOVA_CLEANUP(&) { context.Destroy(); };

    // Create surface and swapchain for GLFW window

    auto swapchain = nova::Swapchain::Create(context, glfwGetWin32Window(window),
        nova::TextureUsage::Storage
        | nova::TextureUsage::TransferDst
        | nova::TextureUsage::ColorAttach,
        nova::PresentMode::Immediate);
    NOVA_CLEANUP(&) { swapchain.Destroy(); };

    // Create required Nova objects

    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto cmdPool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);
    auto state = nova::CommandState::Create(context);
    auto heap = nova::DescriptorHeap::Create(context);
    NOVA_CLEANUP(&) {
        cmdPool.Destroy();
        fence.Destroy();
        state.Destroy();
        heap.Destroy();
    };

    // Shaders - Compute

    auto computeShader = nova::Shader::Create(context, nova::ShaderStage::Compute, {
        nova::shader::ComputeKernel(Vec3U(16u, 16u, 1u), R"glsl(
            ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
            vec2 uv = vec2(pos) / (vec2(gl_NumWorkGroups.xy) * vec2(gl_WorkGroupSize.xy));
            imageStore(StorageImage2D<rgba8>[0], ivec2(gl_GlobalInvocationID.xy), vec4(uv, 0.5, 1.0));
        )glsl"),
    });
    NOVA_CLEANUP(&) { computeShader.Destroy(); };

    // Shaders - Raster

    auto vertexShader = nova::Shader::Create(context, nova::ShaderStage::Vertex, {
        nova::shader::Output("uv", nova::ShaderVarType::Vec2),
        nova::shader::Kernel(R"glsl(
            uv = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
            gl_Position = vec4(uv * vec2(2.0) - vec2(1.0), 0.0, 1.0);
        )glsl"),
    });
    NOVA_CLEANUP(&) { vertexShader.Destroy(); };

    auto fragmentShader = nova::Shader::Create(context, nova::ShaderStage::Fragment, {
        nova::shader::Input("uv", nova::ShaderVarType::Vec2),
        nova::shader::Output("fragColor", nova::ShaderVarType::Vec4),
        nova::shader::Kernel(R"glsl(
            fragColor = vec4(uv, 0.5, 1.0);
        )glsl"),
    });
    NOVA_CLEANUP(&) { fragmentShader.Destroy(); };

// -----------------------------------------------------------------------------
//                               Main Loop
// -----------------------------------------------------------------------------

    constexpr bool useCompute = true;

    auto lastTime = std::chrono::steady_clock::now();
    auto frames = 0;
    NOVA_CLEANUP(&) { fence.Wait(); };
    while (!glfwWindowShouldClose(window)) {

        // Debug output statistics
        frames++;
        auto newTime = std::chrono::steady_clock::now();
        if (newTime - lastTime > 1s) {
            NOVA_LOG("Frametime = {:.3f} ({} fps)", 1e6 / frames, frames);
            lastTime = std::chrono::steady_clock::now();
            frames = 0;
        }

        // Wait for previous frame and acquire new swapchain image

        fence.Wait();
        queue.Acquire({swapchain}, {fence});
        auto target = swapchain.GetCurrent();

        // Start new command buffer

        cmdPool.Reset();
        auto cmd = cmdPool.Begin(state);

        cmd.BeginRendering({{}, Vec2U(target.GetExtent())}, {target});
        cmd.SetGraphicsState({vertexShader, fragmentShader}, {
            .cullMode = nova::CullMode::None,
        });
        cmd.Draw(3, 1, 0, 0);
        cmd.EndRendering();

        if (useCompute) {

            // Transition ready for writing compute output

            cmd.Transition(target,
                nova::TextureLayout::GeneralImage,
                nova::PipelineStage::Compute);

            // Push swapchain image and TLAS descriptors

            heap.WriteStorageTexture({ 0, nova::DescriptorType::StorageTexture }, target);
            cmd.BindDescriptorHeap(nova::BindPoint::Compute, heap);

            // Trace rays

            cmd.SetComputeState(computeShader);
            cmd.Dispatch(Vec3U((Vec2U(target.GetExtent()) + 15u) / 16u, 1));

        } else {
            cmd.BeginRendering({{}, Vec2U(target.GetExtent())}, {target});
            cmd.SetGraphicsState({vertexShader, fragmentShader}, {
                .cullMode = nova::CullMode::None,
            });
            cmd.Draw(3, 1, 0, 0);
            cmd.EndRendering();
        }

        // Submit and present work

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        // Wait for window events

        glfwPollEvents();
    }
}