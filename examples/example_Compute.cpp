#include "example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/vulkan/glsl/nova_VulkanGlsl.hpp>
#include <nova/rhi/vulkan/hlsl/nova_VulkanHlsl.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <stb_image.h>

NOVA_EXAMPLE(Compute, "compute")
{

// -----------------------------------------------------------------------------
//                             GLFW Initialization
// -----------------------------------------------------------------------------

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(720, 720, "Nova - Compute", nullptr, nullptr);
    NOVA_CLEANUP(&) { glfwTerminate(); };

    i32 width, height;
    glfwGetFramebufferSize(window, &width, &height);

// -----------------------------------------------------------------------------
//                             Nova Initialization
// -----------------------------------------------------------------------------

    auto context = nova::Context::Create({
        .debug = true,
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
    auto fence = nova::Fence::Create(context);
    auto waitValues = std::array { 0ull, 0ull };
    auto commandPools = std::array {
        nova::CommandPool::Create(context, queue),
        nova::CommandPool::Create(context, queue)
    };
    auto heap = nova::DescriptorHeap::Create(context, 3);
    NOVA_CLEANUP(&) {
        commandPools[0].Destroy();
        commandPools[1].Destroy();
        fence.Destroy();
        heap.Destroy();
    };

    // Image

    auto sampler = nova::Sampler::Create(context, nova::Filter::Linear, nova::AddressMode::Edge, {}, 16.f);
    NOVA_CLEANUP(&) { sampler.Destroy(); };
    heap.WriteSampler(0, sampler);

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

        texture.Set({}, texture.GetExtent(), imageData);
        texture.Transition(nova::TextureLayout::Sampled);
        heap.WriteSampledTexture(1, texture);
    }

    // Shaders

    struct PushConstants
    {
        u32 sampledIdx;
        u32 samplerIdx;
        u32  targetIdx;
        Vec2      size;
    };

    auto computeShader = nova::Shader::Create(context, nova::ShaderStage::Compute, "main",
        nova::hlsl::Compile(nova::ShaderStage::Compute, "main", "", {R"hlsl(
            [[vk::binding(0, 0)]] RWTexture2D<float4> RWImage2DF4[];
            [[vk::binding(0, 0)]] Texture2D Image2D[];
            [[vk::binding(0, 0)]] SamplerState Sampler[];
            struct PushConstants {
                uint sampledIdx;
                uint samplerIdx;
                uint  targetIdx;
                float2     size;
            };
            [[vk::push_constant]] ConstantBuffer<PushConstants> pc;
            [numthreads(16, 16, 1)]
            void main(uint2 id: SV_DispatchThreadID) {
                float2 uv = float2(id) / pc.size;
                float3 source = Image2D[pc.sampledIdx].SampleLevel(Sampler[pc.samplerIdx], uv, 0).rgb;
                RWImage2DF4[pc.targetIdx][id] = float4(source, 1.0);
            }
        )hlsl"}));
    NOVA_CLEANUP(&) { computeShader.Destroy(); };

    // auto computeShader = nova::Shader::Create(context, nova::ShaderStage::Compute, "main",
    //     nova::glsl::Compile(nova::ShaderStage::Compute, "", {R"glsl(
    //         #version 460
    //         #extension GL_EXT_scalar_block_layout  : require
    //         #extension GL_EXT_nonuniform_qualifier : require
    //         #extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
    //         #extension GL_EXT_shader_image_load_formatted  : require

    //         layout(set = 0, binding = 0) uniform image2D RWImage2D[];
    //         layout(set = 0, binding = 0) uniform texture2D Image2D[];
    //         layout(set = 0, binding = 0) uniform sampler Sampler[];

    //         layout(push_constant, scalar) uniform PushConstants {
    //             uint sampledIdx;
    //             uint samplerIdx;
    //             uint  targetIdx;
    //             vec2       size;
    //         } pc;

    //         layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
    //         void main() {
    //             ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    //             vec2 uv = vec2(pos) / pc.size;
    //             vec3 source = texture(sampler2D(Image2D[pc.sampledIdx], Sampler[pc.samplerIdx]), uv).rgb;
    //             imageStore(RWImage2D[pc.targetIdx], pos, vec4(source, 1.0));
    //         }
    //     )glsl"}));
    // NOVA_CLEANUP(&) { computeShader.Destroy(); };

// -----------------------------------------------------------------------------
//                               Main Loop
// -----------------------------------------------------------------------------

    auto lastTime = std::chrono::steady_clock::now();
    auto frames = 0;
    NOVA_CLEANUP(&) { fence.Wait(); };
    while (!glfwWindowShouldClose(window)) {

        // Debug output statistics
        int fif = frames++ % 2;
        auto newTime = std::chrono::steady_clock::now();
        if (newTime - lastTime > 1s) {
            NOVA_LOG("Frametime = {:.3f} ({} fps)", 1e6 / frames, frames);
            lastTime = std::chrono::steady_clock::now();
            frames = 0;
        }

        // Wait for previous frame and acquire new swapchain image

        fence.Wait(waitValues[fif]);
        queue.Acquire({swapchain}, {fence});
        auto target = swapchain.GetCurrent();

        // Start new command buffer

        commandPools[fif].Reset();
        auto cmd = commandPools[fif].Begin();

        // Transition ready for writing compute output

        cmd.Transition(target,
            nova::TextureLayout::GeneralImage,
            nova::PipelineStage::Compute);

        // Update target descriptor

        heap.WriteStorageTexture(2, target);
        cmd.BindDescriptorHeap(nova::BindPoint::Compute, heap);

        // Dispatch

        cmd.PushConstants(PushConstants {
            .sampledIdx = 1,
            .samplerIdx = 0,
            .targetIdx = 2,
            .size = Vec2(swapchain.GetExtent()),
        });
        cmd.BindShaders({computeShader});
        cmd.Dispatch(Vec3U((Vec2U(target.GetExtent()) + 15u) / 16u, 1));

        // Submit and present work

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        // Wait for window events

        glfwPollEvents();

        waitValues[fif] = fence.GetPendingValue();
    }
}