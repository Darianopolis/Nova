#include "main/example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>

#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Timer.hpp>

#include <nova/window/nova_Window.hpp>

#include <nova/asset/nova_Image.hpp>

NOVA_EXAMPLE(Compute, "compute")
{
    if (args.size() < 2) {
        NOVA_LOG("Usage: <encoding> <file>");
        return;
    }

    auto encoding = args[0];
    {
        bool found_encoding = false;
        for (auto& _enc : { "rgba", "bc1", "bc2", "bc3", "bc4", "bc5", "bc6", "bc7" }) {
            if (encoding == _enc) {
                found_encoding = true;
                break;
            }
        }
        if (!found_encoding) {
            NOVA_LOG("Unrecognized encoding: {}", encoding);
            NOVA_LOG("Encodings: rgba, bc1, bc2, bc3, bc4, bc5, bc6, bc7");
            return;
        }
    }

    auto file = args[1];
    if (!std::filesystem::exists(file)) {
        NOVA_LOG("Could not file: {}", file);
        return;
    }

// -----------------------------------------------------------------------------
//                             GLFW Initialization
// -----------------------------------------------------------------------------

    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Compute")
        .SetSize({ 800, 800 }, nova::WindowPart::Client)
        .Show(true);

// -----------------------------------------------------------------------------
//                             Nova Initialization
// -----------------------------------------------------------------------------

    auto context = nova::Context::Create({
        .debug = true,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    // Create surface and swapchain for GLFW window

    auto swapchain = nova::Swapchain::Create(context, window.NativeHandle(),
        nova::ImageUsage::Storage
        | nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach,
        nova::PresentMode::Immediate);
    NOVA_DEFER(&) { swapchain.Destroy(); };

    // Create required Nova objects

    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);
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
        // Load image

        NOVA_TIMEIT_RESET();
        nova::ImageDescription desc;
        nova::ImageLoadData src;
        nova::Image_Load(&desc, &src, file);
        NOVA_DEFER(&) { src.Destroy(); };
        NOVA_TIMEIT("image-load");

        // Debug source format

        switch (desc.format) {
            break;case nova::ImageFormat::RGBA8:        NOVA_LOG("Loading: RGBA8");
            break;case nova::ImageFormat::RGBA16_Float: NOVA_LOG("Loading: RGBA16_Float");
            break;case nova::ImageFormat::RGBA32_Float: NOVA_LOG("Loading: RGBA32_Float");
            break;case nova::ImageFormat::BC1:          NOVA_LOG("Loading: BC1");
            break;case nova::ImageFormat::BC2:          NOVA_LOG("Loading: BC2");
            break;case nova::ImageFormat::BC3:          NOVA_LOG("Loading: BC3");
            break;case nova::ImageFormat::BC4:          NOVA_LOG("Loading: BC4");
            break;case nova::ImageFormat::BC5:          NOVA_LOG("Loading: BC5");
            break;case nova::ImageFormat::BC6:          NOVA_LOG("Loading: BC6");
            break;case nova::ImageFormat::BC7:          NOVA_LOG("Loading: BC7");
        }

        // Resize window

        Vec3U extent = { desc.width, desc.height, 0u };
        window.SetSize({ desc.width, desc.height }, nova::WindowPart::Client);

        // Select target format

        auto target_desc = desc;
        target_desc.is_signed = false;
        nova::Format format;
        if (encoding == "rgba") {
            target_desc.format = nova::ImageFormat::RGBA8;
            format = nova::Format::RGBA8_UNorm;
        } else if (encoding == "bc1") {
            target_desc.format = nova::ImageFormat::BC1;
            format = nova::Format::BC1A_UNorm;
        } else if (encoding == "bc2") {
            target_desc.format = nova::ImageFormat::BC2;
            format = nova::Format::BC2_UNorm;
        } else if (encoding == "bc3") {
            target_desc.format = nova::ImageFormat::BC3;
            format = nova::Format::BC3_UNorm;
        } else if (encoding == "bc4") {
            target_desc.format = nova::ImageFormat::BC4;
            format = nova::Format::BC4_UNorm;
        } else if (encoding == "bc5") {
            target_desc.format = nova::ImageFormat::BC5;
            format = nova::Format::BC5_UNorm;
        } else if (encoding == "bc6") {
            target_desc.format = nova::ImageFormat::BC6;
            format = nova::Format::BC6_UFloat;
        } else if (encoding == "bc7") {
            target_desc.format = nova::ImageFormat::BC7;
            format = nova::Format::BC7_Unorm;
        }

        // Encode

        NOVA_TIMEIT_RESET();
        nova::ImageAccessor target_accessor(target_desc);
        std::vector<b8> data(target_accessor.GetSize());
        nova::Image_Copy(desc, src.data, target_accessor, data.data());
        NOVA_TIMEIT("image-encode");

        // Upload to GPU

        NOVA_TIMEIT_RESET();
        image = nova::Image::Create(context,
            extent,
            nova::ImageUsage::Sampled | nova::ImageUsage::TransferDst,
            format);
        image.Set({}, image.Extent(), data.data());
        image.Transition(nova::ImageLayout::Sampled);
        NOVA_TIMEIT("image-upload");
    }

    // Shaders

    struct PushConstants
    {
        u32   image;
        u32 sampler;
        u32  target;
        Vec2   size;
    };

    auto hlsl_shader = nova::Shader::Create(context, nova::ShaderLang::Hlsl, nova::ShaderStage::Compute, "main", "", {
        // language=hlsl
        R"hlsl(
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
    float4 source = Image2D[pc.image].SampleLevel(Sampler[pc.linear_sampler], uv, 0);
    float4 dest = float4(1, 0, 1, 1);
    float3 color = lerp(dest.rgb, source.rgb, source.a);
    RWImage2DF4[pc.target][id] = float4(color, 1);
}
        )hlsl"
    });
    NOVA_DEFER(&) { hlsl_shader.Destroy(); };

    auto glsl_shader = nova::Shader::Create(context, nova::ShaderLang::Glsl, nova::ShaderStage::Compute, "main", "", {
        // language=glsl
        R"glsl(
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
    vec4 source = texture(sampler2D(Image2D[pc.image], Sampler[pc.linear_sampler]), uv);
    vec4 dest = vec4(1, 0, 1, 1);
    vec3 color = mix(dest.rgb, source.rgb, source.a);
    imageStore(RWImage2D[pc.target], pos, vec4(color, 1));
}
        )glsl"
    });
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
    while (app.ProcessEvents()) {

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
        auto target = swapchain.Target();

        // Start new command buffer

        command_pools[fif].Reset();
        auto cmd = command_pools[fif].Begin();

        // Transition ready for writing compute output

        cmd.Transition(target,
            nova::ImageLayout::GeneralImage,
            nova::PipelineStage::Compute);

        // Dispatch

        cmd.PushConstants(PushConstants {
            .image = image.Descriptor(),
            .sampler = sampler.Descriptor(),
            .target = swapchain.Target().Descriptor(),
            .size = Vec2(swapchain.Extent()),
        });
        cmd.BindShaders({shaders[frame_index % 2]});
        cmd.Dispatch(Vec3U((Vec2U(target.Extent()) + 15u) / 16u, 1));

        // Submit and present work

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        wait_values[fif] = fence.PendingValue();
    }
}