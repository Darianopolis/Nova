#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/rhi/nova_RHI.hpp>

#include <nova/window/nova_Window.hpp>

NOVA_EXAMPLE(RayTracing, "tri-rt")
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
    auto cmd_pool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);
    auto builder = nova::AccelerationStructureBuilder::Create(context);
    NOVA_DEFER(&) {
        cmd_pool.Destroy();
        fence.Destroy();
        builder.Destroy();
    };

// -----------------------------------------------------------------------------
//                        Descriptors & Pipeline
// -----------------------------------------------------------------------------

    // Create the ray gen shader to draw a shaded triangle based on barycentric interpolation

     auto closest_hit_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::ClosestHit, "main", "", {
         // language=glsl
         R"glsl(
 #extension GL_EXT_ray_tracing                      : require
 #extension GL_EXT_scalar_block_layout              : require
 #extension GL_EXT_shader_explicit_arithmetic_types : require
 #extension GL_EXT_nonuniform_qualifier             : require

 layout(location = 0) rayPayloadInEXT vec3 payload;
 hitAttributeEXT vec3 bary;

 void main() {
     payload = vec3(1.0 - bary.x - bary.y, bary.x, bary.y);
 }
         )glsl"
     });
     NOVA_DEFER(&) { closest_hit_shader.Destroy(); };

     auto ray_gen_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::RayGen, "main", "", {
         // language=glsl
         R"glsl(
#extension GL_EXT_ray_tracing                      : require
#extension GL_EXT_shader_image_load_formatted      : require
#extension GL_EXT_scalar_block_layout              : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_nonuniform_qualifier             : require

layout(set = 0, binding = 1) uniform image2D RWImage2D[];

layout(location = 0) rayPayloadEXT vec3 payload;

layout(push_constant, scalar) uniform pc_ {
    uint64_t tlas;
    uint   target;
} pc;

void main() {
    vec3 pos = vec3(vec2(gl_LaunchIDEXT.xy), 1);
    vec3 dir = vec3(0, 0, -1);

    payload = vec3(0.1);
    traceRayEXT(accelerationStructureEXT(pc.tlas), 0, 0xFF, 0, 0, 0, pos, 0, dir, 2, 0);

    imageStore(RWImage2D[pc.target], ivec2(gl_LaunchIDEXT.xy), vec4(payload, 1));
}
          )glsl"
     });
     NOVA_DEFER(&) { ray_gen_shader.Destroy(); };

    auto ray_query_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::Compute, "main", "", {
        // language=glsl
        R"glsl(
#extension GL_EXT_ray_tracing                      : require
#extension GL_EXT_shader_image_load_formatted      : require
#extension GL_EXT_scalar_block_layout              : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_nonuniform_qualifier             : require
#extension GL_EXT_ray_query                        : require

layout(set = 0, binding = 1) uniform image2D RWImage2D[];

layout(push_constant, scalar) uniform pc_ {
    uint64_t tlas;
    uint   target;
    uvec2    size;
} pc;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
    ivec2 tpos = ivec2(gl_GlobalInvocationID.xy);
    if (tpos.x > pc.size.x || tpos.y > pc.size.y) {
        return;
    }
    vec3 pos = vec3(vec2(tpos), 1);
    vec3 dir = vec3(0, 0, -1);
    vec3 color = vec3(0.1);

    rayQueryEXT hit;
    rayQueryInitializeEXT(hit, accelerationStructureEXT(pc.tlas), 0, 0xFF, pos, 0, dir, 2);
    while (rayQueryProceedEXT(hit)) {
        if (rayQueryGetIntersectionTypeEXT(hit, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
            rayQueryConfirmIntersectionEXT(hit);
        }
    }
    if (rayQueryGetIntersectionTypeEXT(hit, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
        vec2 bary = rayQueryGetIntersectionBarycentricsEXT(hit, true);
        color = vec3(1.0 - bary.x - bary.y, bary.x, bary.y);
    }

    imageStore(RWImage2D[pc.target], tpos, vec4(color, 1));
}
        )glsl"
    });
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

        auto cmd = cmd_pool.Begin();
        cmd.BuildAccelerationStructure(builder, uncompacted_blas, scratch);
        queue.Submit({cmd}, {}, {fence});
        fence.Wait();

        // Compact BLAS

        auto blas = nova::AccelerationStructure::Create(context,
            builder.CompactSize(),
            nova::AccelerationStructureType::BottomLevel);

        cmd = cmd_pool.Begin();
        cmd.CompactAccelerationStructure(blas, uncompacted_blas);
        queue.Submit({cmd}, {}, {fence});
        fence.Wait();

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

    NOVA_DEFER(&) { fence.Wait(); };
    while (app.ProcessEvents()) {

        // Wait for previous frame and acquire new swapchain image

        fence.Wait();
        queue.Acquire({swapchain}, {fence});

        // Start new command buffer

        cmd_pool.Reset();
        auto cmd = cmd_pool.Begin();

        // Build scene TLAS

        builder.WriteInstance(instances.HostAddress(), 0, blas,
            glm::scale(Mat4(1), Vec3(swapchain.Extent(), 1.f)),
            0, 0xFF, 0, {});
        cmd.BuildAccelerationStructure(builder, tlas, scratch);

        // Transition ready for writing ray trace output

        cmd.Barrier(nova::PipelineStage::AccelBuild, nova::PipelineStage::RayTracing | nova::PipelineStage::Compute);
        cmd.Transition(swapchain.Target(),
            nova::ImageLayout::GeneralImage,
            nova::PipelineStage::RayTracing | nova::PipelineStage::Compute);

        // Trace rays

        struct PushConstants
        {
            u64 tlas;
            u32 target;
            Vec2U size;
        };

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
        use_ray_query = !use_ray_query;

        // Submit and present work

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        // Wait for window events

        app.WaitForEvents();
    }
}