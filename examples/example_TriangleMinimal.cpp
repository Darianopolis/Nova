#include "example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/vulkan/glsl/nova_VulkanGlsl.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

NOVA_EXAMPLE(TriangleMinimal, "tri-min")
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1080, "Nova - Triangle Minimal", nullptr, nullptr);
    NOVA_CLEANUP(&) { glfwTerminate(); };

    auto context = nova::Context::Create({ .debug = false });
    auto swapchain = nova::Swapchain::Create(context, glfwGetWin32Window(window),
        nova::TextureUsage::ColorAttach
        | nova::TextureUsage::TransferDst
        | nova::TextureUsage::Storage,
        nova::PresentMode::Fifo);
    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto cmdPool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);

    auto vertexShader = nova::Shader::Create(context, nova::ShaderStage::Vertex, "main",
        nova::glsl::Compile(nova::ShaderStage::Vertex, "main", "", {R"glsl(
            #extension GL_EXT_scalar_block_layout  : require
            layout(location = 0) out vec3 color;
            const vec2 positions[3] = vec2[] (vec2(-0.6, 0.6), vec2(0.6, 0.6), vec2(0, -0.6));
            const vec3    colors[3] = vec3[] (vec3(1, 0, 0),   vec3(0, 1, 0),  vec3(0, 0, 1));
            layout(push_constant, scalar) uniform pc_ {
                vec2 offset;
            } pc;
            void main() {
                color = colors[gl_VertexIndex];
                gl_Position = vec4(positions[gl_VertexIndex] + pc.offset, 0.5, 1);
            }
        )glsl"}));

    auto fragmentShader = nova::Shader::Create(context, nova::ShaderStage::Fragment, "main",
        nova::glsl::Compile(nova::ShaderStage::Fragment, "main", "", {R"glsl(
            layout(location = 0) in vec3 inColor;
            layout(location = 0) out vec4 fragColor;
            void main() {
                fragColor = vec4(inColor, 1);
            }
        )glsl"}));

    auto w = context->dlss_render_size.x;
    auto h = context->dlss_render_size.y;

    auto intermediate_image = nova::Texture::Create(context,
        { 1920, 1080, 0 },
        nova::TextureUsage::ColorAttach
        | nova::TextureUsage::Sampled,
        nova::Format::RGBA8_UNorm,
        nova::TextureFlags::None);

    // auto output_test = nova::Texture::Create(context,
    //     { 1920, 1080, 0 },
    //     nova::TextureUsage::Storage,
    //     nova::Format::RGBA8_UNorm,
    //     nova::TextureFlags::None);

    auto depth_image = nova::Texture::Create(context,
        { 1920, 1080, 0 },
        nova::TextureUsage::DepthStencilAttach
        | nova::TextureUsage::Sampled,
        nova::Format::D32_SFloat);

    auto motion_buffers = nova::Texture::Create(context,
        { 1920, 1080, 0 },
        nova::TextureUsage::Sampled,
        nova::Format::RG16_SFloat);

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<f32> dist{-0.5f, 0.5f};

    using namespace std::chrono;
    auto last_time = steady_clock::now();

    u32 m_FrameIndex = 0;
    while (!glfwWindowShouldClose(window)) {

        fence.Wait();
        queue.Acquire({swapchain}, {fence});
        cmdPool.Reset();
        auto cmd = cmdPool.Begin();

        // intermediate_image = swapchain.GetCurrent();

        auto GetCurrentPixelOffset = [&]()
        {
            // Halton jitter
            Vec2 Result(0.0f, 0.0f);

            constexpr int BaseX = 2;
            int Index = m_FrameIndex + 1;
            float InvBase = 1.0f / BaseX;
            float Fraction = InvBase;
            while (Index > 0)
            {
                Result.x += (Index % BaseX) * Fraction;
                Index /= BaseX;
                Fraction *= InvBase;
            }

            constexpr int BaseY = 3;
            Index = m_FrameIndex + 1;
            InvBase = 1.0f / BaseY;
            Fraction = InvBase;
            while (Index > 0)
            {
                Result.y += (Index % BaseY) * Fraction;
                Index /= BaseY;
                Fraction *= InvBase;
            }

            Result.x -= 0.5f;
            Result.y -= 0.5f;
            return Result;
        };
        Vec2 jitter = GetCurrentPixelOffset() * 1.f;
        // Vec2 jitter = { dist(rng), dist(rng) };
        m_FrameIndex = (m_FrameIndex + 1) % 8;

        Vec2 scaled_jitter = jitter / Vec2(w, h);
        scaled_jitter *= 1.f;

        // NOVA_LOG("Jitter: ({}, {})", jitter.x, jitter.y);

        cmd.BeginRendering({{}, Vec2U(w, h)}, {intermediate_image}, depth_image);
        cmd.ClearColor(0, Vec4(Vec3(0.1f), 1.f), Vec2U(w, h));
        cmd.ClearDepth(1.f, Vec2U(w, h));
        cmd.ResetGraphicsState();
        cmd.SetViewports({{{}, Vec2I(w, h)}}, true);
        // cmd.SetDepthState(true, true, nova::CompareOp::Less);
        cmd.SetBlendState({false});
        cmd.BindShaders({vertexShader, fragmentShader});
        cmd.PushConstants(scaled_jitter);
        cmd.Draw(3, 1, 0, 0);
        cmd.EndRendering();

        cmd.ClearColor(motion_buffers, Vec4(0.f, 0.f, 0.f, 0.f));

        NVSDK_NGX_Resource_VK in_color = NVSDK_NGX_Create_ImageView_Resource_VK(
            intermediate_image->view, intermediate_image->image,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            VK_FORMAT_R8G8B8A8_UNORM,
            1920, 1080,
            false);

        auto target = swapchain.GetCurrent();
        // auto target = output_test;
        NVSDK_NGX_Resource_VK out_color = NVSDK_NGX_Create_ImageView_Resource_VK(
            target->view, target->image,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            VK_FORMAT_R8G8B8A8_UNORM,
            1920, 1080,
            true);

        NVSDK_NGX_Resource_VK in_depth = NVSDK_NGX_Create_ImageView_Resource_VK(
            depth_image->view, depth_image->image,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            VK_FORMAT_D32_SFLOAT,
            1920, 1080,
            false);

        NVSDK_NGX_Resource_VK in_motion = NVSDK_NGX_Create_ImageView_Resource_VK(
            motion_buffers->view, motion_buffers->image,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            VK_FORMAT_R16G16_SFLOAT,
            1920, 1080,
            false);

        auto now = steady_clock::now();
        [[maybe_unused]] auto delta_ms = duration_cast<duration<float, std::milli>>(now - last_time).count();
        last_time = now;

        // NOVA_LOG("Upscale ({} x {}) -> ({} x {})", w, h, swapchain.GetExtent().x, swapchain.GetExtent().y);

        NVSDK_NGX_VK_DLSS_Eval_Params params;
        memset(&params, 0, sizeof(params));
        // params.GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_ALBEDO] = &in_color;
        params.pInDepth = &in_depth;
        params.pInMotionVectors = &in_motion;
        // params.InColorSubrectBase.X = 0;
        // params.InColorSubrectBase.Y = 0;
        // params.InToneMapperType = NVSDK_NGX_TONEMAPPER_REINHARD;
        params.InJitterOffsetX = jitter.x;
        params.InJitterOffsetY = jitter.y;
        params.InRenderSubrectDimensions.Width = w;
        params.InRenderSubrectDimensions.Height = h;
        // params.InFrameTimeDeltaInMsec = delta_ms;
        // params.InRenderSubrectDimensions.Width = 1920;
        // params.InRenderSubrectDimensions.Height = 1080;

        params.Feature.pInColor = &in_color;
        params.Feature.pInOutput = &out_color;
        params.InMVScaleX = 1.f;
        params.InMVScaleY = 1.f;
        // params.Feature.InSharpness = 0.35f;

        cmd->Transition(intermediate_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
        cmd->Transition(depth_image,        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
        cmd->Transition(motion_buffers,     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
        cmd->Transition(target,             VK_IMAGE_LAYOUT_GENERAL,                  VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

        auto res = NGX_VULKAN_EVALUATE_DLSS_EXT(cmd->buffer, context->dlss_feature_handle, context->dlss_params, &params);
        if (NVSDK_NGX_FAILED(res)) {
            NOVA_LOG("Failed to evaluate DLSS, {:#x}", u32(res));
        }

        // cmd.Barrier(nova::PipelineStage::All, nova::PipelineStage::All);
        // cmd.BlitImage(swapchain.GetCurrent(), intermediate_image, nova::Filter::Nearest);
        // cmd.Barrier(nova::PipelineStage::All, nova::PipelineStage::All);

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        glfwPollEvents();
    }
}