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

    auto surface = context->CreateSurface(glfwGetWin32Window(window));
    auto swapchain = context->CreateSwapchain(surface,
        TextureUsage::Storage
        | TextureUsage::TransferDst,
        PresentMode::Fifo);
    NOVA_ON_SCOPE_EXIT(&) {
        context->DestroySwapchain(swapchain);
        context->DestroySurface(surface);
    };

    // Create required Nova objects

    auto queue = context->graphics;
    auto cmdPool = context->CreateCommandPool();
    auto fence = context->CreateFence();
    auto tracker = context->CreateResourceTracker();
    auto builder = context->CreateAccelerationStructureBuilder();
    NOVA_ON_SCOPE_EXIT(&) {
        context->DestroyCommandPool(cmdPool);
        context->DestroyFence(fence);
        context->DestroyResourceTracker(tracker);
        context->DestroyAccelerationStructureBuilder(builder);
    };

// -----------------------------------------------------------------------------
//                        Descriptors & Pipeline
// -----------------------------------------------------------------------------

    // Create descriptor layout to hold one storage image and acceleration structure

    auto descLayout = context->CreateDescriptorLayout({
        {DescriptorType::StorageTexture},
        {DescriptorType::AccelerationStructure},
    }, true);
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyDescriptorLayout(descLayout); };

    // Create a pipeline layout for the above set layout

    auto pipelineLayout = context->CreatePipelineLayout({}, {descLayout}, nova::BindPoint::RayTracing);
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyPipelineLayout(pipelineLayout); };

    // Create the ray gen shader to draw an shaded triangle based on barycentric interpolation

    auto rayGenShader = context->CreateShader(
        ShaderStage::RayGen, {},
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
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyShader(rayGenShader); };

    // Create a ray tracing pipeline with one ray gen shader

    auto pipeline = context->CreateRayTracingPipeline();
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyRayTracingPipeline(pipeline); };
    pipeline->Update(pipelineLayout, {rayGenShader}, {}, {}, {});

// -----------------------------------------------------------------------------
//                              Triangle BLAS
// -----------------------------------------------------------------------------

    // Vertex data

    auto vertices = context->CreateBuffer(3 * sizeof(Vec3),
        BufferUsage::AccelBuild, BufferFlags::DeviceLocal | BufferFlags::CreateMapped);
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyBuffer(vertices); };
    vertices->Set<Vec3>({ {0.5f, 0.2f, 0.f}, {0.2f, 0.8f, 0.f}, {0.8f, 0.8f, 0.f} });

    // Index data

    auto indices = context->CreateBuffer(3 * sizeof(u32),
        BufferUsage::AccelBuild,
        BufferFlags::DeviceLocal | BufferFlags::CreateMapped);
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyBuffer(indices); };
    indices->Set<u32>({ 0u, 1u, 2u });

    // Configure BLAS build

    builder->SetTriangles(0,
        vertices->address, nova::Format::RGB32F, u32(sizeof(Vec3)), 2,
        indices->address, nova::IndexType::U32, 1);
    builder->Prepare(AccelerationStructureType::BottomLevel, {}, 1);

    // Create BLAS and scratch buffer

    auto blas = context->CreateAccelerationStructure(builder->GetBuildSize(),
        AccelerationStructureType::BottomLevel);
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyAccelerationStructure(blas); };
    auto scratch = context->CreateBuffer(builder->GetBuildScratchSize(),
        BufferUsage::Storage, BufferFlags::DeviceLocal);
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyBuffer(scratch); };

    // Build BLAS

    auto cmd = cmdPool->BeginPrimary(tracker);
    cmd->BuildAccelerationStructure(builder, blas, scratch);
    queue->Submit({cmd}, {}, {fence});
    fence->Wait();

// -----------------------------------------------------------------------------
//                                Scene TLAS
// -----------------------------------------------------------------------------

    // Instance data

    auto instances = context->CreateBuffer(builder->GetInstanceSize(),
        BufferUsage::AccelBuild,
        BufferFlags::DeviceLocal | BufferFlags::CreateMapped);
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyBuffer(instances); };

    // Configure TLAS build

    builder->SetInstances(0, instances->address, 1);
    builder->Prepare(AccelerationStructureType::TopLevel, {}, 1);

    // Create TLAS and resize scratch buffer

    auto tlas = context->CreateAccelerationStructure(builder->GetBuildSize(),
        AccelerationStructureType::TopLevel);
    NOVA_ON_SCOPE_EXIT(&) { context->DestroyAccelerationStructure(tlas); };
    if (builder->GetBuildScratchSize() > scratch->size)
    {
        context->DestroyBuffer(scratch);
        scratch = context->CreateBuffer(builder->GetBuildScratchSize(),
            BufferUsage::Storage, BufferFlags::DeviceLocal);
    }

// -----------------------------------------------------------------------------
//                               Main Loop
// -----------------------------------------------------------------------------

    NOVA_ON_SCOPE_EXIT(&) { fence->Wait(); };
    while (!glfwWindowShouldClose(window))
    {
        // Wait for previous frame and acquire new swapchain image

        fence->Wait();
        queue->Acquire({swapchain}, {fence});

        // Start new command buffer

        cmdPool->Reset();
        cmd = cmdPool->BeginPrimary(tracker);

        // Build scene TLAS

        builder->WriteInstance(instances->mapped, 0, blas,
            glm::scale(Mat4(1), Vec3(Vec2(swapchain->current->extent), 1.f)),
            0, 0xFF, 0, {});
        cmd->BuildAccelerationStructure(builder, tlas, scratch);

        // Transition ready for writing ray trace output

        cmd->Transition(swapchain->current,
            nova::ResourceState::GeneralImage,
            nova::BindPoint::RayTracing);

        // Push swapchain image and TLAS descriptors

        cmd->PushStorageTexture(pipelineLayout, 0, 0, swapchain->current);
        cmd->PushAccelerationStructure(pipelineLayout, 0, 1, tlas);

        // Trace rays

        cmd->BindPipeline(pipeline);
        cmd->TraceRays(pipeline, Vec3U(Vec2U(swapchain->current->extent), 1), 0);

        // Submit and present work

        cmd->Present(swapchain->current);
        queue->Submit({cmd}, {fence}, {fence});
        queue->Present({swapchain}, {fence});

        // Wait for window events

        glfwWaitEvents();
    }
}