#include "main/example_Main.hpp"
#include "example_RayTracing.slang"

#include <nova/rhi/nova_RHI.hpp>
#include <nova/window/nova_Window.hpp>

NOVA_EXAMPLE(RayTracing, "rt")
{
    // Create window

    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Ray Tracing")
        .SetSize({ 1920, 1080 }, nova::WindowPart::Client)
        .Show(true);

    // Create Nova context with ray tracing enabled

    auto context = nova::Context::Create({
        .debug = false,
        .ray_tracing = true,
    });
    NOVA_DEFER(&) { context.Destroy(); };
    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);

    // Create surface and swapchain for GLFW window

    auto swapchain = nova::Swapchain::Create(context, window,
        nova::ImageUsage::Storage
        | nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach,
        nova::PresentMode::Fifo);
    NOVA_DEFER(&) { swapchain.Destroy(); };

    // Create Shaders and Pipeline

    auto ray_gen_shader = nova::Shader::Create(context,     nova::ShaderLang::Slang, nova::ShaderStage::RayGen,     "RayGeneration", "example_RayTracing.slang");
    NOVA_DEFER(&) { ray_gen_shader.Destroy(); };
    auto miss_shader = nova::Shader::Create(context,        nova::ShaderLang::Slang, nova::ShaderStage::Miss,       "Miss",          "example_RayTracing.slang");
    NOVA_DEFER(&) { miss_shader.Destroy(); };
    auto closest_hit_shader = nova::Shader::Create(context, nova::ShaderLang::Slang, nova::ShaderStage::ClosestHit, "ClosestHit",    "example_RayTracing.slang");
    NOVA_DEFER(&) { closest_hit_shader.Destroy(); };

    auto pipeline = nova::RayTracingPipeline::Create(context);
    NOVA_DEFER(&) { pipeline.Destroy(); };
    pipeline.Update(ray_gen_shader, {miss_shader}, {nova::HitShaderGroup{.closesthit_shader = closest_hit_shader}}, {});

    // Create accceleration structure builder

    auto builder = nova::AccelerationStructureBuilder::Create(context);
    NOVA_DEFER(&) { builder.Destroy(); };

    // Create BLASes

    auto scratch = nova::Buffer::Create(context, 0, nova::BufferUsage::Storage, nova::BufferFlags::DeviceLocal);
    NOVA_DEFER(&) { scratch.Destroy(); };

    auto CreateBlas = [&](nova::Span<Vec3> cpu_vertices, nova::Span<u32> cpu_indices) {
        auto vertices = nova::Buffer::Create(context, cpu_vertices.size() * sizeof(Vec3),
            nova::BufferUsage::AccelBuild,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
        NOVA_DEFER(&) { vertices.Destroy(); };
        vertices.Set<Vec3>(cpu_vertices);

        // Index data

        auto indices = nova::Buffer::Create(context, cpu_indices.size() * sizeof(u32),
            nova::BufferUsage::AccelBuild,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
        NOVA_DEFER(&) { indices.Destroy(); };
        indices.Set<u32>(cpu_indices);

        // Configure BLAS build

        builder.AddTriangles(0,
            vertices.DeviceAddress(), nova::Format::RGB32_SFloat, u32(sizeof(Vec3)), u32(cpu_vertices.size()) - 1,
            indices.DeviceAddress(), nova::IndexType::U32, u32(cpu_indices.size()) / 3);
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
    };

    auto quad_blas = CreateBlas({
        {-1, 0, -1}, {-1, 0,  1}, {1, 0, 1},
        {-1, 0, -1}, { 1, 0, -1}, {1, 0, 1},
    }, {
        0, 1, 2, 3, 4, 5
    });
    NOVA_DEFER(&) { quad_blas.Destroy(); };

    auto cube_blas = CreateBlas({
        {-1, -1, -1}, {1, -1, -1}, {-1, 1, -1}, {1, 1, -1},
        {-1, -1,  1}, {1, -1,  1}, {-1, 1,  1}, {1, 1,  1},
    }, {
        4, 6, 0, 2, 0, 6, 0, 1, 4, 5, 4, 1,
        0, 2, 1, 3, 1, 2, 1, 3, 5, 7, 5, 3,
        2, 6, 3, 7, 3, 6, 4, 5, 6, 7, 6, 5
    });
    NOVA_DEFER(&) { cube_blas.Destroy(); };

    // Instance data

    auto instances = nova::Buffer::Create(context, builder.InstanceSize(),
        nova::BufferUsage::AccelBuild,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_DEFER(&) { instances.Destroy(); };

    // Configure TLAS build

    builder.AddInstances(0, instances.DeviceAddress(), 3);
    builder.Prepare(nova::AccelerationStructureType::TopLevel,
        nova::AccelerationStructureFlags::PreferFastTrace, 1);

    // Create TLAS and resize scratch buffer

    auto tlas = nova::AccelerationStructure::Create(context,
        builder.BuildSize(),
        nova::AccelerationStructureType::TopLevel);
    NOVA_DEFER(&) { tlas.Destroy(); };
    scratch.Resize(builder.BuildScratchSize());

    // Main loop

    auto start = std::chrono::steady_clock::now();

    NOVA_DEFER(&) { queue.WaitIdle(); };
    while (app.ProcessEvents()) {

        // Wait for previous frame and acquire new swapchain image

        queue.WaitIdle();
        queue.Acquire({swapchain});

        // Start new command buffer

        auto cmd = queue.Begin();

        // Build scene TLAS

        {
            auto set = [&](u32 idx, nova::AccelerationStructure blas, glm::mat4 mat) {
                builder.WriteInstance(instances.HostAddress(), idx, blas, mat, idx, 0xFF, 0, {});
            };

            auto time = std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::steady_clock::now() - start).count();

            glm::mat4 cube(1.f);
            cube *= glm::translate(glm::mat4(1.f), { -1.5f, 2.f, 2.f });
            cube *= glm::mat4_cast(glm::quat({ time / 2.f, time / 3.f, time / 5.f }));
            set(0, cube_blas, cube);

            glm::mat4 mirror(1.f);
            mirror *= glm::translate(glm::mat4(1.f), { 2.f, 2.f, 2.f });
            mirror *= glm::mat4_cast(glm::angleAxis(glm::sin(time) / 8 + 1, glm::vec3(0.f, 1.f, 0.f)));
            mirror *= glm::mat4_cast(glm::angleAxis(-1.8f, glm::vec3(1.f, 0.f, 0.f)));
            set(1, quad_blas, mirror);

            glm::mat4 floor(1.f);
            floor *= glm::translate(glm::mat4(1.f), {0.f, 0.f, 2.f});
            floor *= glm::scale(glm::mat4(1.f), {5.f, 5.f, 5.f});
            set(2, quad_blas, floor);
        }

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
        });

        cmd.TraceRays(pipeline, Vec3U(swapchain.Extent(), 1));

        // Submit and present work

        cmd.Present(swapchain);
        queue.Submit({cmd}, {});
        queue.Present({swapchain}, {});
    }
}