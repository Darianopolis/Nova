#include "main/example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>

#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Timer.hpp>

#include <nova/window/nova_Window.hpp>

#include <nova/asset/nova_Image.hpp>

#include "example_Compute.slang"

NOVA_EXAMPLE(Compute, "compute")
{
    nova::Log("Running example: Compute");

    if (args.size() < 2) {
        NOVA_THROW_STACKLESS("Usage: <encoding> <file>");
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
            NOVA_THROW_STACKLESS("Unrecognized encoding: {}\n"
                       "Encodings: rgba, bc1, bc2, bc3, bc4, bc5, bc6, bc7",
                       encoding);
        }
    }

    auto file = args[1];
    if (!std::filesystem::exists(file)) {
        NOVA_THROW_STACKLESS("Could not file: {}", file);
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
    std::array<nova::SyncPoint, 2> wait_values;

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
            break;case nova::ImageFormat::RGBA8:        nova::Log("Loading: RGBA8");
            break;case nova::ImageFormat::RGBA16_Float: nova::Log("Loading: RGBA16_Float");
            break;case nova::ImageFormat::RGBA32_Float: nova::Log("Loading: RGBA32_Float");
            break;case nova::ImageFormat::BC1:          nova::Log("Loading: BC1");
            break;case nova::ImageFormat::BC2:          nova::Log("Loading: BC2");
            break;case nova::ImageFormat::BC3:          nova::Log("Loading: BC3");
            break;case nova::ImageFormat::BC4:          nova::Log("Loading: BC4");
            break;case nova::ImageFormat::BC5:          nova::Log("Loading: BC5");
            break;case nova::ImageFormat::BC6:          nova::Log("Loading: BC6");
            break;case nova::ImageFormat::BC7:          nova::Log("Loading: BC7");
        }

        // Resize window

        Vec3U extent = { desc.width, desc.height, 0u };
        window.SetSize({ desc.width, desc.height }, nova::WindowPart::Client);

        // Select target format

        auto target_desc = desc;
        target_desc.is_signed = false;
        nova::Format format;
        if (encoding == "rgba") {
            target_desc.format = nova::ImageFormat::RGBA8; format = nova::Format::RGBA8_UNorm;
        } else if (encoding == "bc1") {
            target_desc.format = nova::ImageFormat::BC1; format = nova::Format::BC1A_UNorm;
        } else if (encoding == "bc2") {
            target_desc.format = nova::ImageFormat::BC2; format = nova::Format::BC2_UNorm;
        } else if (encoding == "bc3") {
            target_desc.format = nova::ImageFormat::BC3; format = nova::Format::BC3_UNorm;
        } else if (encoding == "bc4") {
            target_desc.format = nova::ImageFormat::BC4; format = nova::Format::BC4_UNorm;
        } else if (encoding == "bc5") {
            target_desc.format = nova::ImageFormat::BC5; format = nova::Format::BC5_UNorm;
        } else if (encoding == "bc6") {
            target_desc.format = nova::ImageFormat::BC6; format = nova::Format::BC6_UFloat;
        } else if (encoding == "bc7") {
            target_desc.format = nova::ImageFormat::BC7; format = nova::Format::BC7_Unorm;
        } else {
            nova::Log("Invalid encoding must be one of: rgba, bc[1-7]");
            return;
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

    // Shader

    auto shader = nova::Shader::Create(context, nova::ShaderLang::Slang, nova::ShaderStage::Compute, "Compute", "example_Compute.slang");
    NOVA_DEFER(&) { shader.Destroy(); };

// -----------------------------------------------------------------------------
//                               Main Loop
// -----------------------------------------------------------------------------

    auto last_time = std::chrono::steady_clock::now();
    u64 frame_index = 0;
    u64 frames = 0;
    NOVA_DEFER(&) { queue.WaitIdle(); };
    while (app.ProcessEvents()) {

        // Debug output statistics
        frames++;
        auto new_time = std::chrono::steady_clock::now();
        if (new_time - last_time > 1s) {
            nova::Log("Frametime = {:.3f} ({} fps)", 1e6 / frames, frames);
            last_time = std::chrono::steady_clock::now();
            frames = 0;
        }

        u32 fif = frame_index++ % 2;

        // Wait for previous frame and acquire new swapchain image

        wait_values[fif].Wait();
        queue.Acquire({swapchain}, {});
        auto target = swapchain.Target();

        // Start new command buffer

        auto cmd = queue.Begin();

        // Transition ready for writing compute output

        cmd.Transition(target,
            nova::ImageLayout::GeneralImage,
            nova::PipelineStage::Compute);

        // Dispatch

        cmd.PushConstants(PushConstants {
            .source = {image.Descriptor(), sampler.Descriptor()},
            .target = swapchain.Target().Descriptor(),
            .size = Vec2(swapchain.Extent()),
        });
        cmd.BindShaders(shader);
        cmd.Dispatch(Vec3U((Vec2U(target.Extent()) + 15u) / 16u, 1));

        // Submit and present work

        cmd.Present(swapchain);
        wait_values[fif] = queue.Submit({cmd}, {});
        queue.Present({swapchain}, {});
    }
}