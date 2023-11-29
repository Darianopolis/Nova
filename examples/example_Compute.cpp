#include "main/example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/vulkan/glsl/nova_VulkanGlsl.hpp>
#include <nova/rhi/vulkan/hlsl/nova_VulkanHlsl.hpp>

#include <nova/core/nova_Guards.hpp>

#include <nova/core/win32/nova_MinWinInclude.hpp>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <stb_image.h>

#include <rdo_bc_encoder.h>

NOVA_EXAMPLE(Compute, "compute")
{

// -----------------------------------------------------------------------------
//                             GLFW Initialization
// -----------------------------------------------------------------------------

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(720, 720, "Nova - Compute", nullptr, nullptr);
    NOVA_DEFER(&) { glfwTerminate(); };

// -----------------------------------------------------------------------------
//                             Nova Initialization
// -----------------------------------------------------------------------------

    auto context = nova::Context::Create({
        .debug = true,
        .compatibility = true,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    // Create surface and swapchain for GLFW window

    auto swapchain = nova::Swapchain::Create(context, glfwGetWin32Window(window),
        nova::ImageUsage::Storage
        | nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach,
        nova::PresentMode::Immediate);
    NOVA_DEFER(&) { swapchain.Destroy(); };

    // Create required Nova objects

    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto fence = nova::Fence::Create(context);
    auto wait_values = std::array { 0ull, 0ull };
    auto command_pools = std::array {
        nova::CommandPool::Create(context, queue),
        nova::CommandPool::Create(context, queue)
    };
    NOVA_DEFER(&) {
        command_pools[0].Destroy();
        command_pools[1].Destroy();
        fence.Destroy();
    };

    // Image

    auto sampler = nova::Sampler::Create(context, nova::Filter::Linear, nova::AddressMode::Edge, {}, 16.f);
    NOVA_DEFER(&) { sampler.Destroy(); };

    nova::Image image;
    NOVA_DEFER(&) { image.Destroy(); };
    {
        int width, height, channels;
        auto image_data = stbi_load("assets/textures/statue.jpg", &width, &height, &channels, STBI_rgb_alpha);
        NOVA_DEFER(&) { stbi_image_free(image_data); };

        utils::image_u8 src_image{ u32(width), u32(height) };
        std::memcpy(src_image.get_pixels().data(), image_data, width * height * 4);

        constexpr bool use_bc7 = true;

        if (use_bc7) {
            rdo_bc::rdo_bc_params params;
            params.m_bc7enc_reduce_entropy = true;

            rdo_bc::rdo_bc_encoder encoder;
            encoder.init(src_image, params);
            encoder.encode();

            image = nova::Image::Create(context,
                Vec3U(u32(width), u32(height), 0),
                nova::ImageUsage::Sampled | nova::ImageUsage::TransferDst,
                nova::Format::BC7_Unorm,
                {});

            image.Set({}, image.GetExtent(), encoder.get_blocks());
        } else {
            image = nova::Image::Create(context,
                Vec3U(u32(width), u32(height), 0),
                nova::ImageUsage::Sampled | nova::ImageUsage::TransferDst,
                nova::Format::RGBA8_UNorm,
                {});

            image.Set({}, image.GetExtent(), src_image.get_pixels().data());
        }

        image.Transition(nova::ImageLayout::Sampled);
    }

    // Shaders

    struct PushConstants
    {
        u32   image;
        u32 sampler;
        u32  target;
        Vec2   size;
    };

    auto hlsl_shader = nova::Shader::Create(context, nova::ShaderStage::Compute, "main",
        nova::hlsl::Compile(nova::ShaderStage::Compute, "main", "", {R"hlsl(
            [[vk::binding(0, 0)]] Texture2D               Image2D[];
            [[vk::binding(1, 0)]] RWTexture2D<float4> RWImage2DF4[];
            [[vk::binding(2, 0)]] SamplerState            Sampler[];

            struct PushConstants {
                uint          image;
                uint linear_sampler;
                uint         target;
                float2         size;
            };

            [[vk::push_constant]] ConstantBuffer<PushConstants> pc;

            [numthreads(16, 16, 1)]
            void main(uint2 id: SV_DispatchThreadID) {
                float2 uv = float2(id) / pc.size;
                float3 source = Image2D[pc.image].SampleLevel(Sampler[pc.linear_sampler], uv, 0).rgb;
                RWImage2DF4[pc.target][id] = float4(source, 1.0);
            }
        )hlsl"}));
    NOVA_DEFER(&) { hlsl_shader.Destroy(); };

    auto glsl_shader = nova::Shader::Create(context, nova::ShaderStage::Compute, "main",
        nova::glsl::Compile(nova::ShaderStage::Compute, "main", "", {R"glsl(
            #extension GL_EXT_scalar_block_layout  : require
            #extension GL_EXT_nonuniform_qualifier : require
            #extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
            #extension GL_EXT_shader_image_load_formatted  : require

            layout(set = 0, binding = 0) uniform texture2D Image2D[];
            layout(set = 0, binding = 1) uniform image2D RWImage2D[];
            layout(set = 0, binding = 2) uniform sampler   Sampler[];

            layout(push_constant, scalar) uniform PushConstants {
                uint          image;
                uint linear_sampler;
                uint         target;
                vec2           size;
            } pc;

            layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
            void main() {
                ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
                vec2 uv = vec2(pos) / pc.size;
                vec3 source = texture(sampler2D(Image2D[pc.image], Sampler[pc.linear_sampler]), uv).rgb;
                imageStore(RWImage2D[pc.target], pos, vec4(source, 1.0));
            }
        )glsl"}));
    NOVA_DEFER(&) { glsl_shader.Destroy(); };

    // Alternate shaders each frame

    nova::Shader shaders[] = { glsl_shader, hlsl_shader };

// -----------------------------------------------------------------------------
//                               Main Loop
// -----------------------------------------------------------------------------

    auto last_time = std::chrono::steady_clock::now();
    u64 frame_index = 0;
    u64 frames = 0;
    NOVA_DEFER(&) { fence.Wait(); };
    while (!glfwWindowShouldClose(window)) {

        // Debug output statistics
        frames++;
        auto new_time = std::chrono::steady_clock::now();
        if (new_time - last_time > 1s) {
            NOVA_LOG("Frametime = {:.3f} ({} fps)", 1e6 / frames, frames);
            last_time = std::chrono::steady_clock::now();
            frames = 0;
        }

        u32 fif = frame_index++ % 2;

        // Wait for previous frame and acquire new swapchain image

        fence.Wait(wait_values[fif]);
        queue.Acquire({swapchain}, {fence});
        auto target = swapchain.GetCurrent();

        // Start new command buffer

        command_pools[fif].Reset();
        auto cmd = command_pools[fif].Begin();

        // Transition ready for writing compute output

        cmd.Transition(target,
            nova::ImageLayout::GeneralImage,
            nova::PipelineStage::Compute);

        // Dispatch

        cmd.PushConstants(PushConstants {
            .image = image.GetDescriptor(),
            .sampler = sampler.GetDescriptor(),
            .target = swapchain.GetCurrent().GetDescriptor(),
            .size = Vec2(swapchain.GetExtent()),
        });
        cmd.BindShaders({shaders[frame_index % 2]});
        cmd.Dispatch(Vec3U((Vec2U(target.GetExtent()) + 15u) / 16u, 1));

        // Submit and present work

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        // Wait for window events

        glfwPollEvents();

        wait_values[fif] = fence.GetPendingValue();
    }
}