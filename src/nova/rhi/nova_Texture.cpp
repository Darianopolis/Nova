#include "nova_RHI.hpp"

namespace nova
{
    Texture* Context::CreateTexture(Vec3U size, TextureUsage _usage, Format _format, TextureFlags flags)
    {
        auto texture = new Texture;
        NOVA_ON_SCOPE_FAILURE(&) { DestroyTexture(texture); };
        texture->context = this;

        auto usage = VkImageUsageFlags(_usage);
        auto format = VkFormat(_format);
        bool makeView = (usage & ~(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        texture->mips = flags >= TextureFlags::Mips
            ? 1 + u32(std::log2(f32(std::max(size.x, size.y))))
            : 1;

        VkImageType imageType;
        VkImageViewType viewType;
        texture->layers = 1;

        if (flags >= TextureFlags::Array)
        {
            if (size.z > 0)
            {
                texture->layers = size.z;
                size.z = 1;
                imageType = VK_IMAGE_TYPE_2D;
                viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            }
            else
            if (size.y > 0)
            {
                texture->layers = size.y;
                size.y = 1;
                imageType = VK_IMAGE_TYPE_1D;
                viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            }
            else
            {
                NOVA_THROW("Image array must have at least 1 dimension and 1 layer");
            }
        }
        else
        {
            if (size.z > 0)
            {
                imageType = VK_IMAGE_TYPE_3D;
                viewType = VK_IMAGE_VIEW_TYPE_3D;
            }
            else
            if (size.y > 0)
            {
                imageType = VK_IMAGE_TYPE_2D;
                viewType = VK_IMAGE_VIEW_TYPE_2D;
            }
            else
            if (size.z > 0)
            {
                imageType = VK_IMAGE_TYPE_1D;
                viewType = VK_IMAGE_VIEW_TYPE_1D;
            }
            else
            {
                NOVA_THROW("Image must have at least one non-zero dimension");
            }
        }

        texture->extent = glm::max(size, Vec3U(1));

        // ---- Create image -----

        VkCall(vmaCreateImage(vma,
            Temp(VkImageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = format,
                .extent = { texture->extent.x, texture->extent.y, texture->extent.z },
                .mipLevels = texture->mips,
                .arrayLayers = texture->layers,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = std::array {
                    graphics->family,
                }.data(),
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            }),
            Temp(VmaAllocationCreateInfo { }),
            &texture->image,
            &texture->allocation,
            nullptr));

        // ---- Pick aspects -----

        switch (format)
        {
        break;case VK_FORMAT_S8_UINT:
            texture->aspect = VK_IMAGE_ASPECT_STENCIL_BIT;

        break;case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
            texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

        break;case VK_FORMAT_D16_UNORM_S8_UINT:
              case VK_FORMAT_D24_UNORM_S8_UINT:
              case VK_FORMAT_D32_SFLOAT_S8_UINT:
            texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        break;default:
            texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        // ---- Make view -----

        if (makeView)
        {
            VkCall(vkCreateImageView(device, Temp(VkImageViewCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = texture->image,
                .viewType = viewType,
                .format = format,
                .subresourceRange = { texture->aspect, 0, texture->mips, 0, texture->layers },
            }), pAlloc, &texture->view));
        }

        return texture;
    }

    void Context::DestroyTexture(Texture* texture)
    {
        if (!texture)
            return;

        if (texture->view)
            vkDestroyImageView(device, texture->view, pAlloc);

        if (texture->allocation)
            vmaDestroyImage(vma, texture->image, texture->allocation);

        delete texture;
    }

// -----------------------------------------------------------------------------

    void CommandList::CopyToTexture(Texture* dst, Buffer* src, u64 srcOffset)
    {
        Transition(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT);

        vkCmdCopyBufferToImage(buffer, src->buffer, dst->image, tracker->Get(dst).layout, 1, Temp(VkBufferImageCopy {
            .bufferOffset = srcOffset,
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageExtent = { dst->extent.x, dst->extent.y,  1 },
        }));
    }

    void CommandList::GenerateMips(Texture* texture)
    {
        if (texture->mips == 1)
            return;

        Transition(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT);

        auto& state = tracker->Get(texture);

        int32_t mipWidth = texture->extent.x;
        int32_t mipHeight = texture->extent.y;

        for (uint32_t mip = 1; mip < texture->mips; ++mip)
        {
            vkCmdPipelineBarrier2(buffer, Temp(VkDependencyInfo {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = Temp(VkImageMemoryBarrier2 {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                    .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                    .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                    .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,

                    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .image = texture->image,
                    .subresourceRange = { texture->aspect, mip - 1, 1, 0, 1 },
                }),
            }));

            vkCmdBlitImage(buffer,
                texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, Temp(VkImageBlit {
                    .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 0, 1 },
                    .srcOffsets = { VkOffset3D{}, VkOffset3D{(int32_t)mipWidth, (int32_t)mipHeight, 1} },
                    .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip,     0, 1 },
                    .dstOffsets = { VkOffset3D{}, VkOffset3D{(int32_t)std::max(mipWidth / 2, 1), (int32_t)std::max(mipHeight / 2, 1), 1} },
                }),
                VK_FILTER_LINEAR);

            mipWidth = std::max(mipWidth / 2, 1);
            mipHeight = std::max(mipHeight / 2, 1);
        }

        vkCmdPipelineBarrier2(buffer, Temp(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = Temp(VkImageMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,

                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .image = texture->image,
                .subresourceRange = { texture->aspect, texture->mips - 1, 1, 0, 1 },
            }),
        }));

        state.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    void CommandList::Transition(Texture* texture, VkImageLayout newLayout, VkPipelineStageFlags2 newStages, VkAccessFlags2 newAccess)
    {
        auto& state = tracker->Get(texture);

        if (state.layout == newLayout && state.stage == newStages && state.access == newAccess)
            return;

        // vkCmdPipelineBarrier2(buffer, Temp(VkDependencyInfo {
        //     .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        //     .imageMemoryBarrierCount = 1,
        //     .pImageMemoryBarriers = Temp(VkImageMemoryBarrier2 {
        //         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

        //         .srcStageMask = state.stage,
        //         .srcAccessMask = state.access,
        //         .dstStageMask = newStages,
        //         .dstAccessMask = newAccess,

        //         .oldLayout = state.layout,
        //         .newLayout = newLayout,
        //         .image = texture->image,
        //         .subresourceRange = { texture->aspect, 0, texture->mips, 0, texture->layers },
        //     })
        // }));

        // TODO: Temporary fix

        vkCmdPipelineBarrier2(buffer, Temp(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = Temp(VkImageMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = 0,

                .oldLayout = state.layout,
                .newLayout = newLayout,
                .image = texture->image,
                .subresourceRange = { texture->aspect, 0, texture->mips, 0, texture->layers },
            })
        }));

        state.layout = newLayout;
        state.stage = newStages;
        state.access = newAccess;
    }

// -----------------------------------------------------------------------------

    void CommandList::Clear(Texture* texture, Vec4 color)
    {
        Transition(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_CLEAR_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT);

        vkCmdClearColorImage(buffer,
            texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            nova::Temp(VkClearColorValue {{ color.r, color.g, color.b, color.a }}),
            1, nova::Temp(VkImageSubresourceRange { VK_IMAGE_ASPECT_COLOR_BIT, 0, texture->mips, 0, texture->layers }));
    }

// -----------------------------------------------------------------------------

    ResourceTracker* Context::CreateResourceTracker()
    {
        return new ResourceTracker;
    }

    void Context::DestroyResourceTracker(ResourceTracker* tracker)
    {
        delete tracker;
    }
}