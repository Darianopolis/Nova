#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
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
        .debug = true,
        .ray_tracing = true,
    });
    NOVA_DEFER(&) { context.Destroy(); };
    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);

    // Create surface and swapchain for GLFW window

    auto swapchain = nova::Swapchain::Create(context, window.NativeHandle(),
        nova::ImageUsage::Storage
        | nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach,
        nova::PresentMode::Fifo);
    NOVA_DEFER(&) { swapchain.Destroy(); };

    // Create the ray gen shader to draw a shaded triangle based on barycentric interpolation

    struct PushConstants
    {
        u64                     tlas;
        nova::ImageDescriptor target;
    };

    auto preamble = R"glsl(
#extension GL_EXT_ray_tracing                      : require
#extension GL_EXT_shader_image_load_formatted      : require
#extension GL_EXT_scalar_block_layout              : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_nonuniform_qualifier             : require

struct Payload
{
    vec3 color;
    bool allow_reflection;
    bool missed;
};

const vec3 Camera = vec3(0, 1.5, -7);
const vec3 Light = vec3(0, 200, 0);
const vec3 SkyTop = vec3(0.24, 0.44, 0.72);
const vec3 SkyBottom = vec3(0.75, 0.86, 0.93);

layout(set = 0, binding = 1) uniform image2D RWImage2D[];

layout(push_constant, scalar) uniform pc_ {
    uint64_t tlas;
    uint   target;
} pc;
    )glsl";

    // Ray generation shader

    auto ray_gen_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::RayGen, "main", "", {
        preamble,
        R"glsl(
layout(location = 0) rayPayloadEXT Payload payload;

void main() {
    uvec2 idx = gl_LaunchIDEXT.xy;
    vec2 size = vec2(gl_LaunchSizeEXT.xy);

    vec2 uv = idx / size;
    vec3 target = vec3((uv.x * 2 - 1) * 1.8 * (size.x / size.y),
                       (1 - uv.y) * 4 - 2 + Camera.y,
                       0);

    payload.allow_reflection = true;
    payload.missed = false;
    traceRayEXT(accelerationStructureEXT(pc.tlas), 0, 0xFF, 0, 0, 0, Camera, 0.001, target - Camera, 1000, 0);

    imageStore(RWImage2D[pc.target & 0xFFFFF], ivec2(gl_LaunchIDEXT.xy), vec4(payload.color, 1));
}
        )glsl"
    });
    NOVA_DEFER(&) { ray_gen_shader.Destroy(); };

    // Miss shader

    auto miss_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::Miss, "main", "", {
        preamble,
        R"glsl(
layout(location = 0) rayPayloadInEXT Payload payload;

void main()
{
    float slope = normalize(gl_WorldRayDirectionEXT).y;
    float t = clamp(slope * 5 + 0.5, 0, 1);
    payload.color = mix(SkyBottom, SkyTop, t);
    payload.missed = true;
}
        )glsl",
    });
    NOVA_DEFER(&) { miss_shader.Destroy(); };

    // Closest hit shader

    auto closest_hit_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::ClosestHit, "main", "", {
        preamble,
        R"glsl(
layout(location = 0) rayPayloadInEXT Payload payload;
layout(location = 1) rayPayloadEXT Payload shadow_payload;
hitAttributeEXT vec3 uv;

void HitCube()
{
    uint tri = gl_PrimitiveID;
    tri /= 2;
    vec3 normal = vec3(uvec3(equal(uvec3(tri) % 3, uvec3(0, 1, 2))) * (tri < 3 ? -1 : 1));
    vec3 world_normal = normalize(normal * mat3(gl_ObjectToWorld3x4EXT));
    vec3 color = abs(normal) / 3 + 0.5;
    if (uv.x < 0.03 || uv.y < 0.03) {
        color = vec3(0.25);
    }
    color *= clamp(dot(world_normal, normalize(Light)), 0, 1) + 0.33;
    payload.color = color;
}

void HitMirror()
{
    if (!payload.allow_reflection) {
        payload.color = vec3(0, 0, 0);
        return;
    }

    vec3 pos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_RayTmaxEXT;
    vec3 normal = normalize(vec3(0, 1, 0) * mat3(gl_ObjectToWorld3x4EXT));
    vec3 reflected = reflect(normalize(gl_WorldRayDirectionEXT), normal);

    payload.allow_reflection = false;
    traceRayEXT(accelerationStructureEXT(pc.tlas), 0, 0xFF, 0, 0, 0, pos, 0.001, reflected, 1000, 0);
}

void HitFloor()
{
    vec3 pos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_RayTmaxEXT;

    bvec2 pattern = greaterThan(fract(pos.xz), vec2(0.5));
    payload.color = (pattern.x != pattern.y) ? vec3(0.6) : vec3(0.4);

    shadow_payload.allow_reflection = false;
    shadow_payload.missed = false;
    traceRayEXT(accelerationStructureEXT(pc.tlas), 0, 0xFF, 0, 0, 0, pos, 0.001, Light - pos, 1000, 1);

    if (!shadow_payload.missed) {
        payload.color /= 2;
    }
}

void main()
{
    switch (gl_InstanceID) {
        case 0: HitCube(); break;
        case 1: HitMirror(); break;
        case 2: HitFloor(); break;
        default: payload.color = vec3(1, 0, 1); break;
    }
}
        )glsl"
    });
    NOVA_DEFER(&) { closest_hit_shader.Destroy(); };

    // Create ray tracing pipeline

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