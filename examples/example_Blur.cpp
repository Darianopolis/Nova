#include "main/example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>
#include <nova/window/nova_Window.hpp>
#include <nova/asset/nova_Image.hpp>

#include "example_Blur.slang"

f32 sigma = 3.f;

f32 Gaussian(f32 x)
{
    f32 Numerator = 1.f;
    f32 deviation = sigma;
    f32 denominator = f32(glm::sqrt(2.f * glm::pi<f32>()) * deviation);

    f32 exponentNumerator = -x * x;
    f32 exponentDenominator = float(2.f * (deviation * deviation));

    f32 left = Numerator / denominator;
    f32 right = float(glm::exp(exponentNumerator / exponentDenominator));

    return left * right;
}

std::vector<f32> CreateGaussianKernel(u32 radius)
{
    u32 size = radius * 2 + 1;
    std::vector<f32> kernel(size * 2 + 1);
    f32 sum = 0.f;

    f32 midpoint = f32(radius);
    for (u32 i = 0; i < size; ++i) {
        f32 x = i - midpoint;
        f32 gx = Gaussian(x);
        sum += gx;
        kernel[i] = gx;
    }

    for (u32 i = 0; i < size; ++i) {
        kernel[i] /= sum;
    }

    return kernel;
}

NOVA_EXAMPLE(Blur, "blur")
{
    nova::Log("Running example: Blur");

    if (args.size() < 1) {
        NOVA_THROW_STACKLESS("Usage: <file>");
    }

    auto file = args[0];
    if (!nova::fs::exists(file)) {
        NOVA_THROW_STACKLESS("Could not file: {}", file);
    }

// -----------------------------------------------------------------------------
//                             GLFW Initialization
// -----------------------------------------------------------------------------

    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Blur")
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

    auto swapchain = nova::Swapchain::Create(context, window,
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
        nova::ImageDescription desc = {};
        nova::ImageLoadData src = {};
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

NOVA_DEBUG();
        NOVA_DEBUGEXPR(desc.width);
        NOVA_DEBUGEXPR(desc.height);
        NOVA_DEBUGEXPR(desc.layers);
        NOVA_DEBUGEXPR(desc.mips);
        NOVA_DEBUGEXPR(src.data);
        NOVA_DEBUGEXPR(src.offset);
        NOVA_DEBUGEXPR(src.size);

        // Select target format

        auto target_desc = desc;
        target_desc.is_signed = false;
        nova::Format format;
        target_desc.format = nova::ImageFormat::RGBA8; format = nova::Format::RGBA8_UNorm;

        // Encode

        NOVA_TIMEIT_RESET();
        nova::ImageAccessor target_accessor(target_desc);
        std::vector<b8> data(target_accessor.GetSize());
NOVA_DEBUG();
        nova::Image_Copy(desc, src.data, target_accessor, data.data());
NOVA_DEBUG();
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

    nova::Image secondaries[2];
    for (u32 i = 0; i < std::size(secondaries); ++i) {
        secondaries[i] = nova::Image::Create(context,
            Vec3(Vec2(image.Extent()), 0),
            nova::ImageUsage::Sampled | nova::ImageUsage::Storage,
            image.Format());
    }
    NOVA_DEFER(&) { for (auto secondary : secondaries) secondary.Destroy(); };

    nova::Buffer kernel_buffer = nova::Buffer::Create(context, 0, nova::BufferUsage::Storage, nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    NOVA_DEFER(&) { kernel_buffer.Destroy(); };

    // Shader

    auto shader = nova::Shader::Create(context, nova::ShaderLang::Slang, nova::ShaderStage::Compute, "Compute", "example_Blur.slang");
    NOVA_DEFER(&) { shader.Destroy(); };

// -----------------------------------------------------------------------------
//                               Main Loop
// -----------------------------------------------------------------------------

    i32 blur_radius = 0;
    i32 passes = 1;
    app.AddCallback([&](const nova::AppEvent& event) {
        if (event.type == nova::EventType::MouseScroll) {
            blur_radius = std::max(0, blur_radius + i32(event.scroll.scrolled.y));
            nova::Log("Blur Radius: {}", blur_radius);
        }
        if (event.type == nova::EventType::Input) {
            auto vk = app.ToVirtualKey(event.input.channel);
            if (vk == nova::VirtualKey::E) {
                passes++;
            } else if (vk == nova::VirtualKey::Q) {
                passes = std::max(2, passes) - 1;
            }
            nova::Log("Passes: {}", passes);
        }
    });

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

        cmd.BindShaders(shader);

        auto kernel = CreateGaussianKernel(blur_radius);
        kernel_buffer.Resize(kernel.size() * sizeof(f32));
        std::memcpy(kernel_buffer.HostAddress(), kernel.data(), kernel_buffer.Size());

        for (int i = 0; i < passes * 2; ++i) {
            bool horizontal = i % 2;

            auto source = secondaries[i % 2];
            target = secondaries[1 - (i % 2)];

            if (i == 0) source = image;
            if (i == passes * 2 - 1) target = swapchain.Target();

            cmd.PushConstants(PushConstants {
                .source = {source.Descriptor(), sampler.Descriptor()},
                .target = target.Descriptor(),
                .size = Vec2(swapchain.Extent()),
                .horizontal = horizontal,
                .blur_radius = blur_radius,
                .kernel = (const f32*)kernel_buffer.DeviceAddress(),
            });
            cmd.Dispatch(Vec3U((Vec2U(target.Extent()) + 15u) / 16u, 1));
            cmd.Barrier(nova::PipelineStage::Compute, nova::PipelineStage::Compute);
        }

        // cmd.PushConstants(PushConstants {
        //     .source = {image.Descriptor(), sampler.Descriptor()},
        //     .target = secondary.Descriptor(),
        //     .size = Vec2(swapchain.Extent()),
        //     .horizontal = 1,
        //     .blur_radius = blur_radius,
        //     .kernel = (const f32*)kernel_buffer.DeviceAddress(),
        // });
        // cmd.Dispatch(Vec3U((Vec2U(target.Extent()) + 15u) / 16u, 1));
        // cmd.Barrier(nova::PipelineStage::Compute, nova::PipelineStage::Compute);
        // cmd.PushConstants(PushConstants {
        //     .source = {secondary.Descriptor(), sampler.Descriptor()},
        //     .target = swapchain.Target().Descriptor(),
        //     .size = Vec2(swapchain.Extent()),
        //     .horizontal = 0,
        //     .blur_radius = blur_radius,
        //     .kernel = (const f32*)kernel_buffer.DeviceAddress(),
        // });
        // cmd.Dispatch(Vec3U((Vec2U(target.Extent()) + 15u) / 16u, 1));

        // Submit and present work

        cmd.Present(swapchain);
        wait_values[fif] = queue.Submit({cmd}, {});
        queue.Present({swapchain}, {});
    }
}