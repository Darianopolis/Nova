#include "example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/vulkan/glsl/nova_VulkanGlsl.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

NOVA_EXAMPLE(RayTracing, "tri-rt")
{
// -----------------------------------------------------------------------------
//                             GLFW Initialization
// -----------------------------------------------------------------------------

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1200, "Nova - Triangle Ray Traced", nullptr, nullptr);
    NOVA_CLEANUP(&) { glfwTerminate(); };

    i32 width, height;
    glfwGetFramebufferSize(window, &width, &height);

// -----------------------------------------------------------------------------
//                             Nova Initialization
// -----------------------------------------------------------------------------

    // Create Nova context with ray tracing enabled

    auto context = nova::Context::Create({
        .debug = true,
        .rayTracing = true,
    });
    NOVA_CLEANUP(&) { context.Destroy(); };

    // Create surface and swapchain for GLFW window

    auto swapchain = nova::Swapchain::Create(context, glfwGetWin32Window(window),
        nova::TextureUsage::Storage | nova::TextureUsage::TransferDst,
        nova::PresentMode::Fifo);
    NOVA_CLEANUP(&) { swapchain.Destroy(); };

    // Create required Nova objects

    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto cmdPool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);
    auto builder = nova::AccelerationStructureBuilder::Create(context);
    auto heap = nova::DescriptorHeap::Create(context, 1);
    NOVA_CLEANUP(&) {
        cmdPool.Destroy();
        fence.Destroy();
        builder.Destroy();
        heap.Destroy();
    };

// -----------------------------------------------------------------------------
//                        Descriptors & Pipeline
// -----------------------------------------------------------------------------

    // Create the ray gen shader to draw a shaded triangle based on barycentric interpolation

    auto rayGenShader = nova::Shader::Create(context, nova::ShaderStage::RayGen, "main",
        nova::glsl::Compile(nova::ShaderStage::RayGen, "", {
            R"glsl(
                #version 460
                #extension GL_EXT_ray_tracing                            : require
                #extension GL_NV_shader_invocation_reorder               : require
                #extension GL_EXT_shader_image_load_formatted            : require
                #extension GL_EXT_scalar_block_layout                    : require
                #extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

                layout(set = 0, binding = 0) uniform image2D RWImage2D[];

                layout(location = 0) rayPayloadEXT uint     payload;
                layout(location = 0) hitObjectAttributeNV vec3 bary;

                layout(push_constant, scalar) uniform pc_ {
                    uint64_t tlas;
                } pc;

                void main() {
                    vec3 pos = vec3(vec2(gl_LaunchIDEXT.xy), 1);
                    vec3 dir = vec3(0, 0, -1);
                    hitObjectNV hit;
                    hitObjectTraceRayNV(hit, accelerationStructureEXT(pc.tlas), 0, 0xFF, 0, 0, 0, pos, 0, dir, 2, 0);

                    vec3 color = vec3(0.1);
                    if (hitObjectIsHitNV(hit)) {


                        hitObjectGetAttributesNV(hit, 0);
                        color = vec3(1.0 - bary.x - bary.y, bary.x, bary.y);
                    }
                    imageStore(RWImage2D[0], ivec2(gl_LaunchIDEXT.xy), vec4(color, 1));
                }
        )glsl"}));
    NOVA_CLEANUP(&) { rayGenShader.Destroy(); };

    // Create a ray tracing pipeline with one ray gen shader

    auto pipeline = nova::RayTracingPipeline::Create(context);
    NOVA_CLEANUP(&) { pipeline.Destroy(); };
    pipeline.Update({rayGenShader}, {}, {}, {});

    // pipeline.WriteHandle(sbt.GetMapped(), 0, 0);

// -----------------------------------------------------------------------------
//                              Triangle BLAS
// -----------------------------------------------------------------------------

    // Vertex data

    auto scratch = nova::Buffer::Create(context, 0, nova::BufferUsage::Storage, nova::BufferFlags::DeviceLocal);
    NOVA_CLEANUP(&) { scratch.Destroy(); };

    auto blas = [&] {
        auto vertices = nova::Buffer::Create(context, 3 * sizeof(Vec3),
            nova::BufferUsage::AccelBuild,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
        NOVA_CLEANUP(&) { vertices.Destroy(); };
        vertices.Set<Vec3>({ {0.5f, 0.2f, 0.f}, {0.2f, 0.8f, 0.f}, {0.8f, 0.8f, 0.f} });

        // Index data

        auto indices = nova::Buffer::Create(context, 3 * sizeof(u32),
            nova::BufferUsage::AccelBuild,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
        NOVA_CLEANUP(&) { indices.Destroy(); };
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
        NOVA_CLEANUP(&) { uncompactedBlas.Destroy(); };
        scratch.Resize(builder.GetBuildScratchSize());

        // Build BLAS

        auto cmd = cmdPool.Begin();
        cmd.BuildAccelerationStructure(builder, uncompactedBlas, scratch);
        queue.Submit({cmd}, {}, {fence});
        fence.Wait();

        // Compact BLAS

        auto blas = nova::AccelerationStructure::Create(context,
            builder.GetCompactSize(),
            nova::AccelerationStructureType::BottomLevel);

        cmd = cmdPool.Begin();
        cmd.CompactAccelerationStructure(blas, uncompactedBlas);
        queue.Submit({cmd}, {}, {fence});
        fence.Wait();

        return blas;
    }();
    NOVA_CLEANUP(&) { blas.Destroy(); };

// -----------------------------------------------------------------------------
//                                Scene TLAS
// -----------------------------------------------------------------------------

    // Instance data

    auto instances = nova::Buffer::Create(context, builder.GetInstanceSize(),
        nova::BufferUsage::AccelBuild,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_CLEANUP(&) { instances.Destroy(); };

    // Configure TLAS build

    builder.SetInstances(0, instances.GetAddress(), 1);
    builder.Prepare(nova::AccelerationStructureType::TopLevel,
        nova::AccelerationStructureFlags::PreferFastTrace, 1);

    // Create TLAS and resize scratch buffer

    auto tlas = nova::AccelerationStructure::Create(context,
        builder.GetBuildSize(),
        nova::AccelerationStructureType::TopLevel);
    NOVA_CLEANUP(&) { tlas.Destroy(); };
    scratch.Resize(builder.GetBuildScratchSize());

// -----------------------------------------------------------------------------
//                               Main Loop
// -----------------------------------------------------------------------------

    NOVA_CLEANUP(&) { fence.Wait(); };
    while (!glfwWindowShouldClose(window)) {

        // Wait for previous frame and acquire new swapchain image

        fence.Wait();
        queue.Acquire({swapchain}, {fence});

        // Start new command buffer

        cmdPool.Reset();
        auto cmd = cmdPool.Begin();

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

        cmd.WriteStorageTexture(heap, 0, swapchain.GetCurrent());
        cmd.BindDescriptorHeap(nova::BindPoint::RayTracing, heap);

        // Trace rays

        cmd.PushConstants(tlas.GetAddress());
        cmd.TraceRays(pipeline, Vec3U(swapchain.GetExtent(), 1), 0);

        // Submit and present work

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        // Wait for window events

        glfwWaitEvents();
    }
}