#include <nova/rhi/nova_RHI.hpp>

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

    auto context = nova::Context({
        .debug = true,
        .rayTracing = true,
    });

    // Create surface and swapchain for GLFW window

    auto surface = nova::Surface(context, glfwGetWin32Window(window));
    auto swapchain = nova::Swapchain(context, surface,
        nova::TextureUsage::Storage | nova::TextureUsage::TransferDst,
        nova::PresentMode::Fifo);

    // Create required Nova objects

    auto& queue = context.graphics;
    auto cmdPool = nova::CommandPool(context, queue);
    auto fence = nova::Fence(context);
    auto tracker = nova::ResourceTracker(context);

    auto builder = nova::AccelerationStructureBuilder(context);

// -----------------------------------------------------------------------------
//                        Descriptors & Pipeline
// -----------------------------------------------------------------------------

    // Create descriptor layout to hold one storage image and acceleration structure

    auto descLayout = nova::DescriptorLayout(context, {
        {nova::DescriptorType::StorageTexture},
        {nova::DescriptorType::AccelerationStructure},
    }, true);

    // Create a pipeline layout for the above set layout

    auto pipelineLayout = nova::PipelineLayout(context, {}, {descLayout}, nova::BindPoint::RayTracing);

    // Create the ray gen shader to draw a shaded triangle based on barycentric interpolation

    auto rayGenShader = nova::Shader(context,
        nova::ShaderStage::RayGen, {},
        "raygen",
        R"(
#version 460

#extension GL_EXT_ray_tracing              : require
#extension GL_NV_shader_invocation_reorder : require

layout(set = 0, binding = 0, rgba8) uniform image2D       outImage;
layout(set = 0, binding = 1) uniform accelerationStructureEXT tlas;

layout(location = 0) rayPayloadEXT uint            payload;
layout(location = 0) hitObjectAttributeNV vec3 barycentric;

void main()
{
    vec3 pos = vec3(vec2(gl_LaunchIDEXT.xy), 1);
    vec3 dir = vec3(0, 0, -1);
    hitObjectNV hit;
    hitObjectTraceRayNV(hit, tlas, 0, 0xFF, 0, 0, 0, pos, 0, dir, 2, 0);

    if (hitObjectIsHitNV(hit))
    {
        hitObjectGetAttributesNV(hit, 0);
        vec3 w = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
        vec3 color = vec3(1, 0, 0) * w.x + vec3(0, 1, 0) * w.y + vec3(0, 0, 1) * w.z;
        imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(w, 1));
    }
    else
    {
        imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(vec3(0.1), 1));
    }
}
        )",
        pipelineLayout);

    // Create a ray tracing pipeline with one ray gen shader

    auto pipeline = nova::RayTracingPipeline(context);
    pipeline.Update(pipelineLayout, {rayGenShader}, {}, {}, {});

// -----------------------------------------------------------------------------
//                              Triangle BLAS
// -----------------------------------------------------------------------------

    // Vertex data

    auto vertices = nova::Buffer(context, 3 * sizeof(Vec3),
        nova::BufferUsage::AccelBuild,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);
    vertices.Set<Vec3>({ {0.5f, 0.2f, 0.f}, {0.2f, 0.8f, 0.f}, {0.8f, 0.8f, 0.f} });

    // Index data

    auto indices = nova::Buffer(context, 3 * sizeof(u32),
        nova::BufferUsage::AccelBuild,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);
    indices.Set<u32>({ 0u, 1u, 2u });

    // Configure BLAS build

    builder.SetTriangles(0,
        vertices.address, nova::Format::RGB32F, u32(sizeof(Vec3)), 2,
        indices.address, nova::IndexType::U32, 1);
    builder.Prepare(nova::AccelerationStructureType::BottomLevel,
        nova::AccelerationStructureFlags::PreferFastTrace
        | nova::AccelerationStructureFlags::AllowCompaction, 1);

    // Create BLAS and scratch buffer

    auto uncompactedBlas = nova::AccelerationStructure(context, builder.GetBuildSize(),
        nova::AccelerationStructureType::BottomLevel);
    auto scratch = nova::Buffer(context, builder.GetBuildScratchSize(),
        nova::BufferUsage::Storage, nova::BufferFlags::DeviceLocal);

    // Build BLAS

    auto cmd = cmdPool.Begin(tracker);
    cmd->BuildAccelerationStructure(builder, uncompactedBlas, scratch);
    queue.Submit({cmd}, {}, {fence});
    fence.Wait();

    // Compact BLAS

    auto blas = nova::AccelerationStructure(context, builder.GetCompactSize(),
        nova::AccelerationStructureType::BottomLevel);

    cmd = cmdPool.Begin(tracker);
    cmd->CompactAccelerationStructure(blas, uncompactedBlas);
    queue.Submit({cmd}, {}, {fence});
    fence.Wait();
    uncompactedBlas = {};

// -----------------------------------------------------------------------------
//                                Scene TLAS
// -----------------------------------------------------------------------------

    // Instance data

    auto instances = nova::Buffer(context, builder.GetInstanceSize(),
        nova::BufferUsage::AccelBuild,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);

    // Configure TLAS build

    builder.SetInstances(0, instances.address, 1);
    builder.Prepare(nova::AccelerationStructureType::TopLevel,
        nova::AccelerationStructureFlags::PreferFastTrace, 1);

    // Create TLAS and resize scratch buffer

    auto tlas = nova::AccelerationStructure(context, builder.GetBuildSize(),
        nova::AccelerationStructureType::TopLevel);
    scratch.Resize(builder.GetBuildScratchSize());

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
        cmd = cmdPool.Begin(tracker);

        // Build scene TLAS

        builder.WriteInstance(instances.mapped, 0, blas,
            glm::scale(Mat4(1), Vec3(Vec2(swapchain.GetCurrent().extent), 1.f)),
            0, 0xFF, 0, {});
        cmd->BuildAccelerationStructure(builder, tlas, scratch);

        // Transition ready for writing ray trace output

        cmd->Transition(swapchain.GetCurrent(),
            nova::ResourceState::GeneralImage,
            nova::BindPoint::RayTracing);

        // Push swapchain image and TLAS descriptors

        cmd->PushStorageTexture(pipelineLayout, 0, 0, swapchain.GetCurrent());
        cmd->PushAccelerationStructure(pipelineLayout, 0, 1, tlas);

        // Trace rays

        cmd->BindPipeline(pipeline);
        cmd->TraceRays(pipeline, Vec3U(Vec2U(swapchain.GetCurrent().extent), 1), 0);

        // Submit and present work

        cmd->Present(swapchain.GetCurrent());
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        // Wait for window events

        glfwWaitEvents();
    }
}