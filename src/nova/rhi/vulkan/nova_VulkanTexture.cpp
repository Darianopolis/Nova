#include "nova_VulkanContext.hpp"

namespace nova
{
    Texture VulkanContext::Texture_Create(Vec3U size, TextureUsage usage, Format format, TextureFlags flags)
    {
        (void)size;
        (void)usage;
        (void)format;
        (void)flags;

        return {};
    }

    void VulkanContext::Destroy(Texture id)
    {
        auto& texture = Get(id);
        if (texture.view)
            vkDestroyImageView(device, texture.view, pAlloc);

        if (texture.allocation)
            vmaDestroyImage(vma, texture.image, texture.allocation);

        textures.Return(id);
    }

    Vec3U VulkanContext::Texture_GetExtent(Texture id)
    {
        return Get(id).extent;
    }

    Format VulkanContext::Texture_GetFormat(Texture id)
    {
        return Format(Get(id).format);
    }

// -----------------------------------------------------------------------------

    void VulkanContext::Cmd_Transition(CommandList cmd, Texture hTexture,
        VkImageLayout newLayout, VkPipelineStageFlags2 newStages, VkAccessFlags2 newAccess)
    {
        auto& texture = Get(hTexture);

        auto& state = Get(Get(cmd).state).imageStates[texture.image];

        if (state.layout == newLayout)
            return;

        vkCmdPipelineBarrier2(Get(cmd).buffer, Temp(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = Temp(VkImageMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                // .srcStageMask = state.stage,
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .srcAccessMask = state.access,
                // .dstStageMask = newStages,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = newAccess,

                .oldLayout = state.layout,
                .newLayout = newLayout,
                .image = texture.image,
                .subresourceRange = { texture.aspect, 0, texture.mips, 0, texture.layers },
            })
        }));

        state.layout = newLayout;
        state.stage = newStages;
        state.access = newAccess;
    }

    void VulkanContext::Cmd_Clear(CommandList cmd, Texture hTexture, Vec4 color)
    {
        Cmd_Transition(cmd, hTexture,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_CLEAR_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT);

        auto& texture = Get(hTexture);

        vkCmdClearColorImage(Get(cmd).buffer,
            texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            nova::Temp(VkClearColorValue {{ color.r, color.g, color.b, color.a }}),
            1, nova::Temp(VkImageSubresourceRange {
                VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.mips, 0, texture.layers }));
    }
}