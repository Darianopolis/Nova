#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

#include "example_Main.hpp"

NOVA_EXAMPLE(Copy, "copy")
{
    auto context = nova::Context::Create({
        .debug = false,
    });
    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto cmdPool = nova::CommandPool::Create(context, queue);
    auto fence = nova::Fence::Create(context);
    NOVA_CLEANUP(&) {
        cmdPool.Destroy();
        fence.Destroy();
        context.Destroy();
    };

    std::array<nova::Buffer, 4> buffers;
    std::array<nova::Texture, 4> textures;
    for (u32 i = 0; i < 4; ++i) {
        buffers[i] = nova::Buffer::Create(context, 8192ull * 8192ull * 4, {}, nova::BufferFlags::DeviceLocal);
        textures[i] = nova::Texture::Create(context, { 8192u, 8192u, 0u }, {}, nova::Format::RGBA8_UNorm, {});
    }
    NOVA_CLEANUP(&) {
        for (u32 i = 0; i < 4; ++i) {
            buffers [i].Destroy();
            textures[i].Destroy();
        }
    };

    for (u32 i = 0; i < 10; ++i) {
        auto cmd = cmdPool.Begin();

        for (auto& texture : textures) {
            cmd.Transition(texture, nova::TextureLayout::GeneralImage, nova::PipelineStage::All);
        }

        for (u32 j = 0; j < 100; ++j) {
            for (u32 k = 0; k < 4; ++k) {
                cmd.CopyToBuffer(buffers[k], buffers[(k + 1) % 4], buffers[0].GetSize());

                // vkCmdCopyBufferToImage(cmd->buffer, buffers[k]->buffer, textures[k]->image, VK_IMAGE_LAYOUT_GENERAL, 1, nova::Temp(VkBufferImageCopy {
                //     .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                //     .imageExtent = {8192u, 8192u, 1u},
                // }));

                // vkCmdCopyImage(cmd->buffer, textures[k]->image, VK_IMAGE_LAYOUT_GENERAL, textures[(k + 1) % 4]->image, VK_IMAGE_LAYOUT_GENERAL, 1, nova::Temp(VkImageCopy {
                //     .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                //     .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                //     .extent = {8192u, 8192u, 1u},
                // }));
            }
        }

        NOVA_TIMEIT_RESET();
        queue.Submit({cmd}, {}, {fence});
        fence.Wait();
        NOVA_TIMEIT("transfer");
    }
}