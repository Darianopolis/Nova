#include "example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

void example_RayTracing()
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

    auto context = nova::Context::Create({
        .debug = true,
        .rayTracing = true,
    });
    NOVA_ON_SCOPE_EXIT(&) { context.Destroy(); };

    // Create surface and swapchain for GLFW window


    auto swapchain = nova::Swapchain::Create(context, glfwGetWin32Window(window),
        nova::TextureUsage::Storage | nova::TextureUsage::TransferDst,
        nova::PresentMode::Fifo);
    NOVA_ON_SCOPE_EXIT(&) { swapchain.Destroy(); };

    // Create required Nova objects

    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto cmdPool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);
    auto state = nova::CommandState::Create(context);
    auto builder = nova::AccelerationStructureBuilder::Create(context);
    NOVA_ON_SCOPE_EXIT(&) { 
        cmdPool.Destroy();
        fence.Destroy();
        state.Destroy();
        builder.Destroy();
    };

    NOVA_TIMEIT("base-vulkan-objects");

// -----------------------------------------------------------------------------
//                        Descriptors & Pipeline
// -----------------------------------------------------------------------------

    // Create descriptor layout to hold one storage image and acceleration structure

    auto descLayout = nova::DescriptorSetLayout::Create(context, {
        nova::binding::StorageTexture("outImage", swapchain.GetFormat()),
        nova::binding::AccelerationStructure("tlas"),
    }, true);
    NOVA_ON_SCOPE_EXIT(&) { descLayout.Destroy(); };

    // Create a pipeline layout for the above set layout

    auto pipelineLayout = nova::PipelineLayout::Create(context, {}, {descLayout}, nova::BindPoint::RayTracing);
    NOVA_ON_SCOPE_EXIT(&) { pipelineLayout.Destroy(); };

    // Create the ray gen shader to draw a shaded triangle based on barycentric interpolation

    auto rayGenShader = nova::Shader::Create(context, nova::ShaderStage::RayGen, {
        nova::shader::Layout(pipelineLayout),
        nova::shader::Fragment(R"glsl(
            layout(location = 0) rayPayloadEXT uint     payload;
            layout(location = 0) hitObjectAttributeNV vec3 bary;
        )glsl"),
        nova::shader::Kernel(R"glsl(
            vec3 pos = vec3(vec2(gl_LaunchIDEXT.xy), 1);
            vec3 dir = vec3(0, 0, -1);
            hitObjectNV hit;
            hitObjectTraceRayNV(hit, tlas, 0, 0xFF, 0, 0, 0, pos, 0, dir, 2, 0);

            vec3 color = vec3(0.1);
            if (hitObjectIsHitNV(hit))
            {
                hitObjectGetAttributesNV(hit, 0);
                color = vec3(1.0 - bary.x - bary.y, bary.x, bary.y);
            }
            imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(color, 1));
        )glsl"),
    });
    NOVA_ON_SCOPE_EXIT(&) { rayGenShader.Destroy(); };

    // Create a ray tracing pipeline with one ray gen shader

    auto pipeline = nova::RayTracingPipeline::Create(context);
    NOVA_ON_SCOPE_EXIT(&) { pipeline.Destroy(); };
    pipeline.Update(pipelineLayout, {rayGenShader}, {}, {}, {});

    NOVA_TIMEIT("pipeline");

// -----------------------------------------------------------------------------
//                              Triangle BLAS
// -----------------------------------------------------------------------------

    // Vertex data

    auto scratch = nova::Buffer::Create(context, 0, nova::BufferUsage::Storage, nova::BufferFlags::DeviceLocal);
    NOVA_ON_SCOPE_EXIT(&) { scratch.Destroy(); };

    auto blas = [&] {
        auto vertices = nova::Buffer::Create(context, 3 * sizeof(Vec3),
            nova::BufferUsage::AccelBuild,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
        NOVA_ON_SCOPE_EXIT(&) { vertices.Destroy(); };
        vertices.Set<Vec3>({ {0.5f, 0.2f, 0.f}, {0.2f, 0.8f, 0.f}, {0.8f, 0.8f, 0.f} });

        // Index data

        auto indices = nova::Buffer::Create(context, 3 * sizeof(u32),
            nova::BufferUsage::AccelBuild,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
        NOVA_ON_SCOPE_EXIT(&) { indices.Destroy(); };
        indices.Set<u32>({ 0u, 1u, 2u });

        // Configure BLAS build

        builder.SetTriangles(0,
            vertices.GetAddress(), nova::Format::RGB32_SFloat, u32(sizeof(Vec3)), 2,
            indices.GetAddress(), nova::IndexType::U32, 1);
        builder.Prepare(nova::AccelerationStructureType::BottomLevel,
            nova::AccelerationStructureFlags::PreferFastTrace
            | nova::AccelerationStructureFlags::AllowCompaction, 1);

        // Create BLAS and scratch buffer

        auto uncompactedBlas = nova::AccelerationStructure::Create(context, builder.GetBuildSize(),
            nova::AccelerationStructureType::BottomLevel);
        NOVA_ON_SCOPE_EXIT(&) { uncompactedBlas.Destroy(); };
        scratch.Resize(builder.GetBuildScratchSize());

        // Build BLAS

        auto cmd = cmdPool.Begin(state);
        cmd.BuildAccelerationStructure(builder, uncompactedBlas, scratch);
        queue.Submit({cmd}, {}, {fence});
        fence.Wait();

        // Compact BLAS

        auto blas = nova::AccelerationStructure::Create(context, 
            builder.GetCompactSize(),
            nova::AccelerationStructureType::BottomLevel);

        cmd = cmdPool.Begin(state);
        cmd.CompactAccelerationStructure(blas, uncompactedBlas);
        queue.Submit({cmd}, {}, {fence});
        fence.Wait();

        return blas;
    }();

    NOVA_TIMEIT("blas");

// -----------------------------------------------------------------------------
//                                Scene TLAS
// -----------------------------------------------------------------------------

    // Instance data

    auto instances = nova::Buffer::Create(context, builder.GetInstanceSize(),
        nova::BufferUsage::AccelBuild,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);

    // Configure TLAS build

    builder.SetInstances(0, instances.GetAddress(), 1);
    builder.Prepare(nova::AccelerationStructureType::TopLevel,
        nova::AccelerationStructureFlags::PreferFastTrace, 1);

    // Create TLAS and resize scratch buffer

    auto tlas = nova::AccelerationStructure::Create(context, 
        builder.GetBuildSize(),
        nova::AccelerationStructureType::TopLevel);
    scratch.Resize(builder.GetBuildScratchSize());

    NOVA_TIMEIT("tlas-prepare");

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

        builder.WriteInstance(instances.GetMapped(), 0, blas,
            glm::scale(Mat4(1), Vec3(swapchain.GetExtent(), 1.f)),
            0, 0xFF, 0, {});
        cmd.BuildAccelerationStructure(builder, tlas, scratch);

        // Transition ready for writing ray trace output

        cmd.Barrier(nova::PipelineStage::AccelBuild, nova::PipelineStage::RayTracing);
        cmd.Transition(swapchain.GetCurrent(),
            nova::TextureLayout::GeneralImage,
            nova::PipelineStage::RayTracing);

        // Push swapchain image and TLAS descriptors

        cmd.PushStorageTexture(pipelineLayout, 0, 0, swapchain.GetCurrent());
        cmd.PushAccelerationStructure(pipelineLayout, 0, 1, tlas);

        // Trace rays

        cmd.TraceRays(pipeline, Vec3U(swapchain.GetExtent(), 1), 0);

        // Submit and present work

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        // Wait for window events

        glfwWaitEvents();
    }
}