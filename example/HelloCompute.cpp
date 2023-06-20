#include <nova/rhi/nova_RHI_Impl.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

using namespace nova::types;

int main()
{
// -----------------------------------------------------------------------------
//                             GLFW Initialization
// -----------------------------------------------------------------------------

    // Initial window setup

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1200,
        "Hello Nova RT Triangle", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) {
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    i32 width, height;
    glfwGetFramebufferSize(window, &width, &height);

// -----------------------------------------------------------------------------
//                             Nova Initialization
// -----------------------------------------------------------------------------

    // Create Nova context with ray tracing enabled

    NOVA_TIMEIT_RESET();

    auto context = +nova::Context({
        .debug = true,
        .shaderObjects = false,
    });

    // Create surface and swapchain for GLFW window

    auto surface = +nova::Surface(context, glfwGetWin32Window(window));
    auto swapchain = +nova::Swapchain(context, surface,
        nova::TextureUsage::Storage | nova::TextureUsage::TransferDst,
        nova::PresentMode::Fifo);

    // Create required Nova objects

    auto queue = context.GetQueue(nova::QueueFlags::Graphics);
    auto cmdPool = +nova::CommandPool(context, queue);
    auto fence = +nova::Fence(context);
    auto state = +nova::CommandState(context);

    NOVA_TIMEIT("base-vulkan-objects");

// -----------------------------------------------------------------------------
//                        Descriptors & Pipeline
// -----------------------------------------------------------------------------

    // Create descriptor layout to hold one storage image and acceleration structure

    auto descLayout = +nova::DescriptorSetLayout(context, {
        nova::binding::StorageTexture("outImage", swapchain.GetFormat()),
    }, true);

    // Create a pipeline layout for the above set layout

    auto pipelineLayout = +nova::PipelineLayout(context, {}, {descLayout}, nova::BindPoint::Compute);

    // Create the ray gen shader to draw a shaded triangle based on barycentric interpolation

    auto computeShader = +nova::Shader(context, nova::ShaderStage::Compute, {
        nova::shader_element::DescriptorSet(0, descLayout),
        nova::shader_element::ComputeKernel(Vec3U(1u), R"(
            ivec2 pos = ivec2(gl_WorkGroupID.xy);
            vec2 uv = vec2(pos) / vec2(gl_NumWorkGroups.xy);
            imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(uv, 0.5, 1.0));
        )")
    });

// -----------------------------------------------------------------------------
//                               Main Loop
// -----------------------------------------------------------------------------

    NOVA_ON_SCOPE_EXIT(&) { fence.Wait(); };
    while (!glfwWindowShouldClose(window))
    {
        // Wait for previous frame and acquire new swapchain image

        fence.Wait();
        queue.Acquire({swapchain}, {fence});

        // Start new command buffer

        cmdPool.Reset();
        auto cmd = cmdPool.Begin(state);

        // Build scene TLAS

        // Transition ready for writing ray trace output

        cmd.Transition(swapchain.GetCurrent(),
            nova::ResourceState::GeneralImage,
            nova::PipelineStage::Compute);

        // Push swapchain image and TLAS descriptors

        cmd.PushStorageTexture(pipelineLayout, 0, 0, swapchain.GetCurrent());

        // Trace rays

        cmd.SetComputeState(pipelineLayout, {computeShader});
        cmd.Dispatch(Vec3U(swapchain.GetExtent(), 1));

        // Submit and present work

        cmd.Present(swapchain.GetCurrent());
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        // Wait for window events

        glfwWaitEvents();
    }
}