#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/rhi/nova_RHI.hpp>

#include <nova/window/nova_Window.hpp>

#include "example_TriangleRayTraced.slang"

NOVA_EXAMPLE(TriangleRayTraced, "tri-rt")
{
// -----------------------------------------------------------------------------
//                             GLFW Initialization
// -----------------------------------------------------------------------------

    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Triangle Ray Traced")
        .SetSize({ 1920, 1080 }, nova::WindowPart::Client)
        .Show(true);

// -----------------------------------------------------------------------------
//                             Nova Initialization
// -----------------------------------------------------------------------------

    // Create Nova context with ray tracing enabled

    auto context = nova::Context::Create({
        .debug = true,
        .ray_tracing = true,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    // Create surface and swapchain for GLFW window

    auto swapchain = nova::Swapchain::Create(context, window.NativeHandle(),
        nova::ImageUsage::Storage | nova::ImageUsage::TransferDst,
        nova::PresentMode::Fifo);
    NOVA_DEFER(&) { swapchain.Destroy(); };

    // Create required Nova objects

    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);
    auto builder = nova::AccelerationStructureBuilder::Create(context);
    NOVA_DEFER(&) { builder.Destroy(); };

// -----------------------------------------------------------------------------
//                                 Pipeline
// -----------------------------------------------------------------------------

    auto closest_hit_shader = nova::Shader::Create(context, nova::ShaderLang::Slang, nova::ShaderStage::ClosestHit, "ClosestHit", "example_TriangleRayTraced.slang");
    NOVA_DEFER(&) { closest_hit_shader.Destroy(); };

    auto ray_gen_shader = nova::Shader::Create(context, nova::ShaderLang::Slang, nova::ShaderStage::RayGen, "RayGeneration", "example_TriangleRayTraced.slang");
    NOVA_DEFER(&) { ray_gen_shader.Destroy(); };

    auto ray_query_shader = nova::Shader::Create(context, nova::ShaderLang::Slang, nova::ShaderStage::Compute, "Compute", "example_TriangleRayTraced.slang");
    NOVA_DEFER(&) { ray_query_shader.Destroy(); };

    // Create a ray tracing pipeline with one ray gen shader

    auto pipeline = nova::RayTracingPipeline::Create(context);
    NOVA_DEFER(&) { pipeline.Destroy(); };
    pipeline.Update(ray_gen_shader, {}, {nova::HitShaderGroup{.closesthit_shader = closest_hit_shader}}, {});

// -----------------------------------------------------------------------------
//                              Triangle BLAS
// -----------------------------------------------------------------------------

    // Vertex data

    auto scratch = nova::Buffer::Create(context, 0, nova::BufferUsage::Storage, nova::BufferFlags::DeviceLocal);
    NOVA_DEFER(&) { scratch.Destroy(); };

    auto blas = [&] {
        auto vertices = nova::Buffer::Create(context, 3 * sizeof(Vec3),
            nova::BufferUsage::AccelBuild,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
        NOVA_DEFER(&) { vertices.Destroy(); };
        vertices.Set<Vec3>({ {0.5f, 0.2f, 0.f}, {0.2f, 0.8f, 0.f}, {0.8f, 0.8f, 0.f} });

        // Index data

        auto indices = nova::Buffer::Create(context, 3 * sizeof(u32),
            nova::BufferUsage::AccelBuild,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
        NOVA_DEFER(&) { indices.Destroy(); };
        indices.Set<u32>({ 0u, 1u, 2u });

        // Configure BLAS build

        builder.AddTriangles(0,
            vertices.DeviceAddress(), nova::Format::RGB32_SFloat, u32(sizeof(Vec3)), 2,
            indices.DeviceAddress(), nova::IndexType::U32, 1);
        builder.Prepare(nova::AccelerationStructureType::BottomLevel,
            nova::AccelerationStructureFlags::PreferFastTrace
            | nova::AccelerationStructureFlags::AllowCompaction, 1);

        // Create BLAS and scratch buffer

        auto uncompacted_blas = nova::AccelerationStructure::Create(context, builder.BuildSize(),
            nova::AccelerationStructureType::BottomLevel);
        NOVA_DEFER(&) { uncompacted_blas.Destroy(); };
        scratch.Resize(builder.BuildScratchSize());

        // Build BLAS

        auto cmd = queue.Begin();
        cmd.BuildAccelerationStructure(builder, uncompacted_blas, scratch);
        queue.Submit({cmd}, {}).Wait();

        // Compact BLAS

        auto blas = nova::AccelerationStructure::Create(context,
            builder.CompactSize(),
            nova::AccelerationStructureType::BottomLevel);

        cmd = queue.Begin();
        cmd.CompactAccelerationStructure(blas, uncompacted_blas);
        queue.Submit({cmd}, {}).Wait();

        return blas;
    }();
    NOVA_DEFER(&) { blas.Destroy(); };

// -----------------------------------------------------------------------------
//                                Scene TLAS
// -----------------------------------------------------------------------------

    // Instance data

    auto instances = nova::Buffer::Create(context, builder.InstanceSize(),
        nova::BufferUsage::AccelBuild,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_DEFER(&) { instances.Destroy(); };

    // Configure TLAS build

    builder.AddInstances(0, instances.DeviceAddress(), 1);
    builder.Prepare(nova::AccelerationStructureType::TopLevel,
        nova::AccelerationStructureFlags::PreferFastTrace, 1);

    // Create TLAS and resize scratch buffer

    auto tlas = nova::AccelerationStructure::Create(context,
        builder.BuildSize(),
        nova::AccelerationStructureType::TopLevel);
    NOVA_DEFER(&) { tlas.Destroy(); };
    scratch.Resize(builder.BuildScratchSize());

// -----------------------------------------------------------------------------
//                               Main Loop
// -----------------------------------------------------------------------------

    bool use_ray_query = false;

    NOVA_DEFER(&) { queue.WaitIdle(); };
    while (app.ProcessEvents()) {

        // Wait for previous frame and acquire new swapchain image

        queue.WaitIdle();
        queue.Acquire({swapchain});

        // Start new command buffer

        auto cmd = queue.Begin();

        // Build scene TLAS

        builder.WriteInstance(instances.HostAddress(), 0, blas,
            glm::scale(Mat4(1), Vec3(swapchain.Extent(), 1.f)),
            0, 0xFF, 0, nova::GeometryInstanceFlags::InstanceForceOpaque);
        cmd.BuildAccelerationStructure(builder, tlas, scratch);

        // Transition ready for writing ray trace output

        cmd.Barrier(nova::PipelineStage::AccelBuild, nova::PipelineStage::RayTracing | nova::PipelineStage::Compute);
        cmd.Transition(swapchain.Target(),
            nova::ImageLayout::GeneralImage,
            nova::PipelineStage::RayTracing | nova::PipelineStage::Compute);

        // Trace rays

        cmd.PushConstants(PushConstants {
            .tlas = tlas.DeviceAddress(),
            .target = swapchain.Target().Descriptor(),
            .size = swapchain.Extent(),
        });

        // Use rt pipeline of ray query from compute

        if (use_ray_query) {
            cmd.BindShaders({ ray_query_shader });
            cmd.Dispatch(Vec3U((swapchain.Extent() + Vec2U(15)) / Vec2U(16), 1));
        } else {
            cmd.TraceRays(pipeline, Vec3U(swapchain.Extent(), 1));
        }
        // use_ray_query = !use_ray_query;

        // Submit and present work

        cmd.Present(swapchain);
        queue.Submit({cmd}, {});
        queue.Present({swapchain}, {});

        // Wait for window events

        app.WaitForEvents();
    }
}