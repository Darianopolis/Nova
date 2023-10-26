#include "example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/vulkan/glsl/nova_VulkanGlsl.hpp>
#include <nova/rhi/vulkan/hlsl/nova_VulkanHlsl.hpp>

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

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
    NOVA_CLEANUP(&) { glfwTerminate(); };

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
    auto fence = nova::Fence::Create(context);
    auto waitValues = std::array { 0ull, 0ull };
    auto commandPools = std::array {
        nova::CommandPool::Create(context, queue),
        nova::CommandPool::Create(context, queue)
    };
    NOVA_CLEANUP(&) {
        commandPools[0].Destroy();
        commandPools[1].Destroy();
        fence.Destroy();
    };

    // Image

    auto sampler = nova::Sampler::Create(context, nova::Filter::Linear, nova::AddressMode::Edge, {}, 16.f);
    NOVA_CLEANUP(&) { sampler.Destroy(); };

    nova::Texture texture;
    NOVA_CLEANUP(&) { texture.Destroy(); };
    {
        int width, height, channels;
        // auto imageData = stbi_load("assets/textures/statue.jpg", &width, &height, &channels, STBI_rgb_alpha);
        auto imageData = stbi_load("D:/Dev/Models/Sponza/textures/column_brickwall_01_BaseColor.png", &width, &height, &channels, STBI_rgb_alpha);
        NOVA_CLEANUP(&) { stbi_image_free(imageData); };



        // texture = nova::Texture::Create(context,
        //     Vec3U(u32(x), u32(y), 0),
        //     nova::TextureUsage::Sampled | nova::TextureUsage::TransferDst,
        //     nova::Format::RGBA8_UNorm,
        //     {});

        // texture.Set({}, texture.GetExtent(), imageData);




        // bc7enc_compress_block_init();
        // bc7enc_compress_block_params params;
        // bc7enc_compress_block_params_init(&params);

        // constexpr i32 BlockDim = 4;
        // constexpr i32 BlockSize = BlockDim * BlockDim;

        // struct Block { uc8 data[BlockSize]; };

        // std::vector<Block> blocks;
        // i32 bcWidth  = width / BlockDim;
        // i32 bcHeight = height / BlockDim;
        // blocks.resize(bcWidth * bcHeight);

        // struct Pixel { uc8 data[4]; };
        // Pixel sourceBlock[BlockSize];

        // i32 transparentCount = 0;
        // i32 totalCount = 0;
        // for (i32 bx = 0; bx < bcWidth; ++bx) {
        //     for (i32 by = 0; by < bcHeight; ++by) {
        //         i32 px = bx * BlockDim;
        //         i32 py = by * BlockDim;
        //         for (i32 lx = 0; lx < BlockDim; ++lx) {
        //             for (i32 ly = 0; ly < BlockDim; ++ly) {
        //                 sourceBlock[lx + ly * BlockDim]
        //                     = reinterpret_cast<const Pixel*>(imageData)[(px + lx) + (py + ly) * width];
        //             }
        //         }
        //         if (bc7enc_compress_block(&blocks[bx + by * bcWidth], sourceBlock, &params)) {
        //             transparentCount++;
        //         }
        //         totalCount++;
        //     }
        // }

        // texture = nova::Texture::Create(context,
        //     Vec3U(u32(width), u32(height), 0),
        //     nova::TextureUsage::Sampled | nova::TextureUsage::TransferDst,
        //     nova::Format::BC7_Unorm,
        //     {});

        // NOVA_LOG("Original size: {}", nova::ByteSizeToString(width * height * 4));
        // NOVA_LOG("Compressed size: {}", nova::ByteSizeToString(blocks.size() * sizeof(Block)));
        // NOVA_LOG("Ratio: {:.3f}", f32(width * height * 4) / f32(blocks.size() * sizeof(Block)));
        // NOVA_LOG("Transparent: {} / {}", transparentCount, totalCount);

        // texture.Set({}, texture.GetExtent(), blocks.data());





        constexpr i32 BlockDim = 4;
        constexpr i32 BlockSize = BlockDim * BlockDim;
        struct Block { uc8 data[BlockSize]; };

        utils::image_u8 image{ u32(width), u32(height) };
        // std::memcpy(image.get_pixels().data(), imageData, width * height * 4);

        {

            i32 maxDim = 2048;
            auto pData = imageData;
            auto getIndex = [](i32 x, i32 y, i32 pitch) { return x + y * pitch; };

            i32 factor = std::max(width / maxDim, height / maxDim);
            i32 sWidth = width / factor;
            i32 sHeight = height / factor;
            i32 factor2 = factor * factor;

            image.init(sWidth, sHeight);

#pragma omp parallel for
            for (i32 x = 0; x < sWidth; ++x) {
                for (i32 y = 0; y < sHeight; ++y) {

                    Vec4 acc = {};

                    for (i32 dx = 0; dx < factor; ++dx) {
                        for (i32 dy = 0; dy < factor; ++dy) {
                            auto* pixel = pData + getIndex(x * factor + dx, y * factor + dy, width) * 4;
                            acc.r += pixel[0];
                            acc.g += pixel[1];
                            acc.b += pixel[2];
                            acc.a += pixel[3];
                        }
                    }

                    acc /= f32(factor2);
                    image.get_pixels()[getIndex(x, y, sWidth)]
                        = { u8(acc.r), u8(acc.g), u8(acc.b), u8(acc.a) };
                }
            }

            width = sWidth;
            height = sHeight;
        }

        if (false) {
            texture = nova::Texture::Create(context,
                Vec3U(u32(width), u32(height), 0),
                nova::TextureUsage::Sampled | nova::TextureUsage::TransferDst,
                nova::Format::RGBA8_UNorm,
                {});

            texture.Set({}, texture.GetExtent(), image.get_pixels().data());
        } else {

            rdo_bc::rdo_bc_params params;
            params.m_bc7enc_reduce_entropy = true;

            rdo_bc::rdo_bc_encoder encoder;
            encoder.init(image, params);
            encoder.encode();

            texture = nova::Texture::Create(context,
                Vec3U(u32(width), u32(height), 0),
                nova::TextureUsage::Sampled | nova::TextureUsage::TransferDst,
                nova::Format::BC7_Unorm,
                {});

            texture.Set({}, texture.GetExtent(), encoder.get_blocks());
        }

        texture.Transition(nova::TextureLayout::Sampled);
    }

    // Shaders

    struct PushConstants
    {
        u32   image;
        u32 sampler;
        u32  target;
        Vec2   size;
    };

    // auto computeShader = nova::Shader::Create(context, nova::ShaderStage::Compute, "main",
    //     nova::hlsl::Compile(nova::ShaderStage::Compute, "main", "", {R"hlsl(
    //         [[vk::binding(0, 0)]] RWTexture2D<float4> RWImage2DF4[];
    //         [[vk::binding(0, 0)]] Texture2D Image2D[];
    //         [[vk::binding(0, 0)]] SamplerState Sampler[];
    //         struct PushConstants {
    //             uint sampledIdx;
    //             uint samplerIdx;
    //             uint  targetIdx;
    //             float2     size;
    //         };
    //         [[vk::push_constant]] ConstantBuffer<PushConstants> pc;
    //         [numthreads(16, 16, 1)]
    //         void main(uint2 id: SV_DispatchThreadID) {
    //             float2 uv = float2(id) / pc.size;
    //             float3 source = Image2D[pc.sampledIdx].SampleLevel(Sampler[pc.samplerIdx], uv, 0).rgb;
    //             RWImage2DF4[pc.targetIdx][id] = float4(source, 1.0);
    //         }
    //     )hlsl"}));
    // NOVA_CLEANUP(&) { computeShader.Destroy(); };

    auto computeShader = nova::Shader::Create(context, nova::ShaderStage::Compute, "main",
        nova::glsl::Compile(nova::ShaderStage::Compute, "main", "", {R"glsl(
            #extension GL_EXT_scalar_block_layout  : require
            #extension GL_EXT_nonuniform_qualifier : require
            #extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
            #extension GL_EXT_shader_image_load_formatted  : require

            layout(set = 0, binding = 0) uniform texture2D Image2D[];
            layout(set = 0, binding = 1) uniform image2D RWImage2D[];
            layout(set = 0, binding = 2) uniform sampler Sampler[];

            layout(push_constant, scalar) uniform PushConstants {
                uint image;
                uint linearSampler;
                uint target;
                vec2 size;
            } pc;

            layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
            void main() {
                ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
                vec2 uv = vec2(pos) / pc.size;
                vec3 source = texture(sampler2D(Image2D[pc.image], Sampler[pc.linearSampler]), uv).rgb;
                imageStore(RWImage2D[pc.targetIdx], pos, vec4(source, 1.0));
            }
        )glsl"}));
    NOVA_CLEANUP(&) { computeShader.Destroy(); };

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

        // Dispatch

        cmd.PushConstants(PushConstants {
            .image = texture.GetDescriptor(),
            .sampler = sampler.GetDescriptor(),
            .target = swapchain.GetCurrent().GetDescriptor(),
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