#include "main/Main.hpp"

#include <nova/gpu/RHI.hpp>

NOVA_EXAMPLE(Copy, "copy")
{
    auto context = nova::Context::Create({
        .debug = false,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);

    std::array<nova::Buffer, 4> buffers;
    std::array<nova::Image, 4> images;
    for (u32 i = 0; i < 4; ++i) {
        buffers[i] = nova::Buffer::Create(context, 8192ull * 8192ull * 4, {}, nova::BufferFlags::DeviceLocal);
        images[i] = nova::Image::Create(context, { 8192u, 8192u, 0u }, {}, nova::Format::RGBA8_UNorm, {});
    }
    NOVA_DEFER(&) {
        for (u32 i = 0; i < 4; ++i) {
            buffers [i].Destroy();
            images[i].Destroy();
        }
    };

    for (u32 i = 0; i < 10; ++i) {
        auto cmd = queue.Begin();

        for (auto& image : images) {
            cmd.Transition(image, nova::ImageLayout::GeneralImage, nova::PipelineStage::All);
        }

        for (u32 j = 0; j < 100; ++j) {
            for (u32 k = 0; k < 4; ++k) {
                cmd.CopyToBuffer(buffers[k], buffers[(k + 1) % 4], buffers[0].Size());
            }
        }

        NOVA_TIMEIT_RESET();
        queue.Submit({cmd}, {}).Wait();
        NOVA_TIMEIT("transfer");
    }

    constexpr size_t size = 512ull * 1024 * 1024;
    constexpr size_t count = 1;

    static volatile int foo;

    std::vector<char> sources[count];
    nova::Buffer targets[count];
    for (int i = 0; i < count; ++i) {
        sources[i] = std::vector<char>(size);
        targets[i] = nova::Buffer::Create(context, size,
            {},
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
    }

    for (int j = 0; j < 50; ++j) {
        std::this_thread::sleep_for(100ms);
        using namespace std::chrono;
        auto start = steady_clock::now();
        for (int i = 0; i < count; ++i) {
            auto dst = targets[i].HostAddress();
            auto src = sources[i].data();
            std::memcpy(dst, src, size);
        }
        auto end = steady_clock::now();
        auto seconds = duration_cast<duration<float>>(end - start).count();

        nova::Log("Total time: {:.2f} seconds ({} /s)", seconds, nova::ByteSizeToString(uint64_t((size * count) / seconds)));
    }
}
