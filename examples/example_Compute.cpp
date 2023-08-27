#include "example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <stb_image.h>

struct PushConstants {
    u32 targetID;
    u32 sourceID;
    u32 samplerID;

    static constexpr std::array Layout {
        nova::Member("targetID",  nova::ShaderVarType::U32),
        nova::Member("sourceID",  nova::ShaderVarType::U32),
        nova::Member("samplerID", nova::ShaderVarType::U32),
    };
};

NOVA_EXAMPLE(compute)
{

// -----------------------------------------------------------------------------
//                             GLFW Initialization
// -----------------------------------------------------------------------------

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1200, "Nova - Compute", nullptr, nullptr);
    NOVA_CLEANUP(&) {
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    i32 width, height;
    glfwGetFramebufferSize(window, &width, &height);

// -----------------------------------------------------------------------------
//                             Nova Initialization
// -----------------------------------------------------------------------------

    auto context = nova::Context::Create({
        .debug = false,
    });
    NOVA_CLEANUP(&) { context.Destroy(); };

    // Create surface and swapchain for GLFW window

    auto swapchain = nova::Swapchain::Create(context, glfwGetWin32Window(window),
        nova::TextureUsage::Storage
        | nova::TextureUsage::TransferDst
        | nova::TextureUsage::ColorAttach,
        nova::PresentMode::Immediate);
    NOVA_CLEANUP(&) { swapchain.Destroy(); };

    // Create required Nova objects

    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto cmdPool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);
    auto state = nova::CommandState::Create(context);
    auto heap = nova::DescriptorHeap::Create(context);
    NOVA_CLEANUP(&) {
        cmdPool.Destroy();
        fence.Destroy();
        state.Destroy();
        heap.Destroy();
    };

    // Image

    auto targetID = heap.Acquire(nova::DescriptorType::StorageTexture);

    auto sampler = nova::Sampler::Create(context, nova::Filter::Linear, nova::AddressMode::Repeat, {}, 16.f);
    NOVA_CLEANUP(&) { sampler.Destroy(); };
    auto samplerID = heap.WriteSampler(heap.Acquire(nova::DescriptorType::Sampler), sampler);

    nova::Texture texture;
    NOVA_CLEANUP(&) { texture.Destroy(); };
    {
        int x, y, channels;
        auto imageData = stbi_load("assets/textures/statue.jpg", &x, &y, &channels, STBI_rgb_alpha);
        NOVA_CLEANUP(&) { stbi_image_free(imageData); };

        texture = nova::Texture::Create(context,
            Vec3U(u32(x), u32(y), 0),
            nova::TextureUsage::Sampled | nova::TextureUsage::TransferDst,
            nova::Format::RGBA8_UNorm,
            {});

        auto staging = nova::Buffer::Create(context, 4 * x * y, nova::BufferUsage::TransferSrc, nova::BufferFlags::Mapped);
        NOVA_CLEANUP(&) { staging.Destroy(); };
        staging.Set(Span(imageData, 4 * x * y));

        auto cmd = cmdPool.Begin(state);
        cmd.CopyToTexture(texture, staging);
        queue.Submit({cmd}, {}, {fence});
        fence.Wait();
    }
    auto sourceID = heap.WriteSampledTexture(heap.Acquire(nova::DescriptorType::SampledTexture), texture);

    // Shaders

    auto computeShader = nova::Shader::Create(context, nova::ShaderStage::Compute, {
        nova::shader::PushConstants("pc", PushConstants::Layout),
        nova::shader::ComputeKernel(Vec3U(16u, 16u, 1u), R"glsl(
            ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
            vec2 uv = vec2(pos) / (vec2(gl_NumWorkGroups.xy) * vec2(gl_WorkGroupSize.xy));

            vec3 source = texture(nova::Sampler2D(pc.sourceID, pc.samplerID), uv).xyz;
            imageStore(nova::StorageImage2D<rgba8>[pc.targetID], ivec2(gl_GlobalInvocationID.xy), vec4(source, 1.0));
        )glsl"),
    });
    NOVA_CLEANUP(&) { computeShader.Destroy(); };

// -----------------------------------------------------------------------------
//                               Main Loop
// -----------------------------------------------------------------------------

    auto lastTime = std::chrono::steady_clock::now();
    auto frames = 0;
    NOVA_CLEANUP(&) { fence.Wait(); };
    while (!glfwWindowShouldClose(window)) {

        // Debug output statistics
        frames++;
        auto newTime = std::chrono::steady_clock::now();
        if (newTime - lastTime > 1s) {
            NOVA_LOG("Frametime = {:.3f} ({} fps)", 1e6 / frames, frames);
            lastTime = std::chrono::steady_clock::now();
            frames = 0;
        }

        // Wait for previous frame and acquire new swapchain image

        fence.Wait();
        queue.Acquire({swapchain}, {fence});
        auto target = swapchain.GetCurrent();

        // Start new command buffer

        cmdPool.Reset();
        auto cmd = cmdPool.Begin(state);

        // Transition ready for writing compute output

        cmd.Transition(target,
            nova::TextureLayout::GeneralImage,
            nova::PipelineStage::Compute);

        // Update target descriptor

        heap.WriteStorageTexture(targetID, target);
        cmd.BindDescriptorHeap(nova::BindPoint::Compute, heap);
        cmd.PushConstants(0, sizeof(PushConstants), nova::Temp(PushConstants {
            .targetID  = targetID.ToShaderUInt(),
            .sourceID  = sourceID.ToShaderUInt(),
            .samplerID = samplerID.ToShaderUInt(),
        }));

        // Dispatch

        cmd.SetComputeState(computeShader);
        cmd.Dispatch(Vec3U((Vec2U(target.GetExtent()) + 15u) / 16u, 1));

        // Submit and present work

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        // Wait for window events

        glfwPollEvents();
    }
}