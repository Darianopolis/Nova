#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

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

    std::unique_ptr<nova::Context> ctx = std::make_unique<nova::VulkanContext>(nova::ContextConfig {
        .debug = true,
        .rayTracing = true,
    });

    // Create surface and swapchain for GLFW window


    auto swapchain = ctx->Swapchain_Create(glfwGetWin32Window(window),
        nova::TextureUsage::Storage | nova::TextureUsage::TransferDst,
        nova::PresentMode::Fifo);

    // Create required Nova objects

    auto queue = ctx->Queue_Get(nova::QueueFlags::Graphics);
    auto cmdPool = ctx->Commands_CreatePool(queue);
    auto fence = ctx->Fence_Create();
    auto state = ctx->Commands_CreateState();

    auto builder = ctx->AccelerationStructures_CreateBuilder();

    NOVA_TIMEIT("base-vulkan-objects");

// -----------------------------------------------------------------------------
//                        Descriptors & Pipeline
// -----------------------------------------------------------------------------

    // Create descriptor layout to hold one storage image and acceleration structure

    auto descLayout = ctx->Descriptors_CreateSetLayout({
        nova::binding::StorageTexture("outImage", ctx->Swapchain_GetFormat(swapchain)),
        nova::binding::AccelerationStructure("tlas"),
    }, true);

    // Create a pipeline layout for the above set layout

    auto pipelineLayout = ctx->Pipelines_CreateLayout({}, {descLayout}, nova::BindPoint::RayTracing);

    // Create the ray gen shader to draw a shaded triangle based on barycentric interpolation

    auto rayGenShader = ctx->Shader_Create(nova::ShaderStage::RayGen, {
        nova::shader::Layout(pipelineLayout),
        nova::shader::Fragment(R"(
            layout(location = 0) rayPayloadEXT uint     payload;
            layout(location = 0) hitObjectAttributeNV vec3 bary;
        )"),
        nova::shader::Kernel(R"(
            vec3 pos = vec3(vec2(gl_LaunchIDEXT.xy), 1);
            vec3 dir = vec3(0, 0, -1);
            hitObjectNV hit;
            hitObjectTraceRayNV(hit, tlas, 0, 0xFF, 0, 0, 0, pos, 0, dir, 2, 0);

            if (hitObjectIsHitNV(hit))
            {
                hitObjectGetAttributesNV(hit, 0);
                imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(vec3(1.0 - bary.x - bary.y, bary.x, bary.y), 1));
            }
            else
            {
                imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(vec3(0.1), 1));
            }
        )")
    });

    // Create a ray tracing pipeline with one ray gen shader

    auto pipeline = ctx->RayTracing_CreatePipeline();
    ctx->RayTracing_UpdatePipeline(pipeline, pipelineLayout, {rayGenShader}, {}, {}, {});

    NOVA_TIMEIT("pipeline");

// -----------------------------------------------------------------------------
//                              Triangle BLAS
// -----------------------------------------------------------------------------

    // Vertex data

    auto vertices = ctx->Buffer_Create(3 * sizeof(Vec3),
        nova::BufferUsage::AccelBuild,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    ctx->Buffer_Set<Vec3>(vertices, { {0.5f, 0.2f, 0.f}, {0.2f, 0.8f, 0.f}, {0.8f, 0.8f, 0.f} });

    // Index data

    auto indices = ctx->Buffer_Create(3 * sizeof(u32),
        nova::BufferUsage::AccelBuild,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    ctx->Buffer_Set<u32>(indices, { 0u, 1u, 2u });

    // Configure BLAS build

    ctx->AccelerationStructures_SetTriangles(builder, 0,
        ctx->Buffer_GetAddress(vertices), nova::Format::RGB32F, u32(sizeof(Vec3)), 2,
        ctx->Buffer_GetAddress(indices), nova::IndexType::U32, 1);
    ctx->AccelerationStructures_Prepare(builder,
        nova::AccelerationStructureType::BottomLevel,
        nova::AccelerationStructureFlags::PreferFastTrace
        | nova::AccelerationStructureFlags::AllowCompaction, 1);

    // Create BLAS and scratch buffer

    auto uncompactedBlas = ctx->AccelerationStructures_Create(
        ctx->AccelerationStructures_GetBuildSize(builder),
        nova::AccelerationStructureType::BottomLevel);
    auto scratch = ctx->Buffer_Create(
        ctx->AccelerationStructures_GetBuildScratchSize(builder),
        nova::BufferUsage::Storage, nova::BufferFlags::DeviceLocal);

    // Build BLAS

    auto cmd = ctx->Commands_Begin(cmdPool, state);
    ctx->Cmd_BuildAccelerationStructure(cmd, builder, uncompactedBlas, scratch);
    ctx->Queue_Submit(queue, {cmd}, {}, {fence});
    ctx->Fence_Wait(fence);

    // Compact BLAS

    auto blas = ctx->AccelerationStructures_Create(
        ctx->AccelerationStructures_GetCompactSize(builder),
        nova::AccelerationStructureType::BottomLevel);

    cmd = ctx->Commands_Begin(cmdPool, state);
    ctx->Cmd_CompactAccelerationStructure(cmd, blas, uncompactedBlas);
    ctx->Queue_Submit(queue, {cmd}, {}, {fence});
    ctx->Fence_Wait(fence);

    vertices = {};
    indices = {};
    uncompactedBlas = {};

    NOVA_TIMEIT("blas");

// -----------------------------------------------------------------------------
//                                Scene TLAS
// -----------------------------------------------------------------------------

    // Instance data

    auto instances = ctx->Buffer_Create(ctx->AccelerationStructures_GetInstanceSize(),
        nova::BufferUsage::AccelBuild,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);

    // Configure TLAS build

    ctx->AccelerationStructures_SetInstances(builder, 0, ctx->Buffer_GetAddress(instances), 1);
    ctx->AccelerationStructures_Prepare(builder,
        nova::AccelerationStructureType::TopLevel,
        nova::AccelerationStructureFlags::PreferFastTrace, 1);

    // Create TLAS and resize scratch buffer

    auto tlas = ctx->AccelerationStructures_Create(
        ctx->AccelerationStructures_GetBuildSize(builder),
        nova::AccelerationStructureType::TopLevel);
    ctx->Buffer_Resize(scratch, ctx->AccelerationStructures_GetBuildScratchSize(builder));

    NOVA_TIMEIT("tlas-prepare");

// -----------------------------------------------------------------------------
//                               Main Loop
// -----------------------------------------------------------------------------

    while (!glfwWindowShouldClose(window))
    {
        // Wait for previous frame and acquire new swapchain image

        ctx->Fence_Wait(fence);
        ctx->Queue_Acquire(queue, {swapchain}, {fence});

        // Start new command buffer

        ctx->Commands_Reset(cmdPool);
        cmd = ctx->Commands_Begin(cmdPool, state);

        // Build scene TLAS

        ctx->AccelerationStructures_WriteInstance(ctx->Buffer_GetMapped(instances), 0, blas,
            glm::scale(Mat4(1), Vec3(ctx->Swapchain_GetExtent(swapchain), 1.f)),
            0, 0xFF, 0, {});
        ctx->Cmd_BuildAccelerationStructure(cmd, builder, tlas, scratch);

        // Transition ready for writing ray trace output

        ctx->Cmd_Transition(cmd, ctx->Swapchain_GetCurrent(swapchain),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
            VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT);

        // Push swapchain image and TLAS descriptors

        ctx->Cmd_PushStorageTexture(cmd, pipelineLayout, 0, 0, ctx->Swapchain_GetCurrent(swapchain));
        ctx->Cmd_PushAccelerationStructure(cmd, pipelineLayout, 0, 1, tlas);

        // Trace rays

        ctx->Cmd_TraceRays(cmd, pipeline, Vec3U(ctx->Swapchain_GetExtent(swapchain), 1), 0);

        // Submit and present work

        ctx->Cmd_Present(cmd, swapchain);
        ctx->Queue_Submit(queue, {cmd}, {fence}, {fence});
        ctx->Queue_Present(queue, {swapchain}, {fence});

        // Wait for window events

        glfwWaitEvents();
    }
}