#include <nova/rhi/nova_RHI.hpp>

using namespace nova;

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

    auto context = Context::Create({
        .debug = true,
        .rayTracing = true,
    });
    NOVA_ON_SCOPE_EXIT(&) { Context::Destroy(context); };

    // Create surface and swapchain for GLFW window

    nova::Surface surface(context, glfwGetWin32Window(window));
    nova::Swapchain swapchain(context, &surface,
        TextureUsage::Storage
        | TextureUsage::TransferDst,
        PresentMode::Fifo);

    // Create required Nova objects

    auto queue = context->graphics;
    auto cmdPool = context->CreateCommandPool();
    auto fence = context->CreateFence();
    auto tracker = context->CreateResourceTracker();
    NOVA_ON_SCOPE_EXIT(&) {
        context->DestroyCommandPool(cmdPool);
        context->DestroyFence(fence);
        context->DestroyResourceTracker(tracker);
    };

    nova::AccelerationStructureBuilder builder(context);

// -----------------------------------------------------------------------------
//                        Descriptors & Pipeline
// -----------------------------------------------------------------------------

    // Create descriptor layout to hold one storage image and acceleration structure

    auto descLayout = context->CreateDescriptorLayout({
        {DescriptorType::StorageTexture},
        {DescriptorType::AccelerationStructure},
    }, true);
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyDescriptorLayout(descLayout); };

    auto descLayout2 = context->CreateDescriptorLayout({
        {DescriptorType::StorageTexture},
        {DescriptorType::AccelerationStructure},
    });
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyDescriptorLayout(descLayout2); };

    // Create a pipeline layout for the above set layout

    auto pipelineLayout = context->CreatePipelineLayout({}, {descLayout2, descLayout}, nova::BindPoint::RayTracing);
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyPipelineLayout(pipelineLayout); };

    // Create the ray gen shader to draw an shaded triangle based on barycentric interpolation

    nova::Shader rayGenShader(context,
        ShaderStage::RayGen, {},
        "raygen",
        R"(
#version 460

#extension GL_EXT_ray_tracing              : require
#extension GL_NV_shader_invocation_reorder : require

layout(set = 1, binding = 0, rgba8) uniform image2D       outImage;
layout(set = 1, binding = 1) uniform accelerationStructureEXT tlas;

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

    auto pipeline = context->CreateRayTracingPipeline();
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyRayTracingPipeline(pipeline); };
    pipeline->Update(pipelineLayout, {&rayGenShader}, {}, {}, {});

// -----------------------------------------------------------------------------
//                              Triangle BLAS
// -----------------------------------------------------------------------------

    // Vertex data

    Buffer vertices(context, 3 * sizeof(Vec3),
        BufferUsage::AccelBuild, BufferFlags::DeviceLocal | BufferFlags::CreateMapped);
    vertices.Set<Vec3>({ {0.5f, 0.2f, 0.f}, {0.2f, 0.8f, 0.f}, {0.8f, 0.8f, 0.f} });

    // Index data

    Buffer indices(context, 3 * sizeof(u32),
        BufferUsage::AccelBuild,
        BufferFlags::DeviceLocal | BufferFlags::CreateMapped);
    indices.Set<u32>({ 0u, 1u, 2u });

    // Configure BLAS build

    builder.SetTriangles(0,
        vertices.address, nova::Format::RGB32F, u32(sizeof(Vec3)), 2,
        indices.address, nova::IndexType::U32, 1);
    builder.Prepare(AccelerationStructureType::BottomLevel, {}, 1);

    // Create BLAS and scratch buffer

    auto blas = context->CreateAccelerationStructure(builder.GetBuildSize(),
        AccelerationStructureType::BottomLevel);
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyAccelerationStructure(blas); };
    Buffer scratch(context, builder.GetBuildScratchSize(),
        BufferUsage::Storage, BufferFlags::DeviceLocal);

    // Build BLAS

    auto cmd = cmdPool->BeginPrimary(tracker);
    cmd->BuildAccelerationStructure(&builder, blas, &scratch);
    queue->Submit({cmd}, {}, {fence});
    fence->Wait();

// -----------------------------------------------------------------------------
//                                Scene TLAS
// -----------------------------------------------------------------------------

    // Instance data

    Buffer instances(context, builder.GetInstanceSize(),
        BufferUsage::AccelBuild,
        BufferFlags::DeviceLocal | BufferFlags::CreateMapped);

    // Configure TLAS build

    builder.SetInstances(0, instances.address, 1);
    builder.Prepare(AccelerationStructureType::TopLevel, {}, 1);

    // Create TLAS and resize scratch buffer

    auto tlas = context->CreateAccelerationStructure(builder.GetBuildSize(),
        AccelerationStructureType::TopLevel);
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyAccelerationStructure(tlas); };
    scratch.Resize(builder.GetBuildScratchSize());

// -----------------------------------------------------------------------------
//                               Main Loop
// -----------------------------------------------------------------------------

    NOVA_ON_SCOPE_EXIT(&) { fence->Wait(); };
    while (!glfwWindowShouldClose(window))
    {
        // Wait for previous frame and acquire new swapchain image

        fence->Wait();
        queue->Acquire({&swapchain}, {fence});

        // Start new command buffer

        cmdPool->Reset();
        cmd = cmdPool->BeginPrimary(tracker);

        // Build scene TLAS

        builder.WriteInstance(instances.mapped, 0, blas,
            glm::scale(Mat4(1), Vec3(Vec2(swapchain.current->extent), 1.f)),
            0, 0xFF, 0, {});
        cmd->BuildAccelerationStructure(&builder, tlas, &scratch);

        // Transition ready for writing ray trace output

        cmd->Transition(swapchain.current,
            nova::ResourceState::GeneralImage,
            nova::BindPoint::RayTracing);

        // Push swapchain image and TLAS descriptors

        cmd->PushStorageTexture(pipelineLayout, 1, 0, swapchain.current);
        cmd->PushAccelerationStructure(pipelineLayout, 1, 1, tlas);

        // Trace rays

        cmd->BindPipeline(pipeline);
        cmd->TraceRays(pipeline, Vec3U(Vec2U(swapchain.current->extent), 1), 0);

        // Submit and present work

        cmd->Present(swapchain.current);
        queue->Submit({cmd}, {fence}, {fence});
        queue->Present({&swapchain}, {fence});

        // Wait for window events

        glfwWaitEvents();
    }
}