#include "VulkanBackend.hpp"

namespace pyr
{
    Ref<Image> Context::CreateImage(uvec3 size, VkImageUsageFlags usage, VkFormat format, ImageFlags flags)
    {
        Ref image = new Image;
        image->context = this;

        b8 makeView = usage != 0;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        image->mips = flags >= ImageFlags::Mips
            ? 1 + u32(std::log2(f32(std::max(size.x, size.y))))
            : 1;

        VkImageType imageType;
        VkImageViewType viewType;
        image->layers = 1;

        if (flags >= ImageFlags::Array)
        {
            if (size.z > 0)
            {
                image->layers = size.z;
                size.z = 1;
                imageType = VK_IMAGE_TYPE_2D;
                viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            }
            else if (size.y > 0)
            {
                image->layers = size.y;
                size.y = 1;
                imageType = VK_IMAGE_TYPE_1D;
                viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            }
            else
            {
                PYR_THROW("Image array must have at least 1 dimension and 1 layer");
            }
        }
        else
        {
            if (size.z > 0)
            {
                imageType = VK_IMAGE_TYPE_3D;
                viewType = VK_IMAGE_VIEW_TYPE_3D;
            }
            else if (size.y > 0)
            {
                imageType = VK_IMAGE_TYPE_2D;
                viewType = VK_IMAGE_VIEW_TYPE_2D;
            }
            else if (size.z > 0)
            {
                imageType = VK_IMAGE_TYPE_1D;
                viewType = VK_IMAGE_VIEW_TYPE_1D;
            }
            else
            {
                PYR_THROW("Image must have at least one non-zero dimension");
            }
        }

        image->extent = glm::max(size, uvec3(1));

        // ---- Create image -----

        VkCall(vmaCreateImage(vma,
            Temp(VkImageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = format,
                .extent = { image->extent.x, image->extent.y, image->extent.z },
                .mipLevels = image->mips,
                .arrayLayers = image->layers,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = std::array {
                    graphics.family,
                }.data(),
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            }),
            Temp(VmaAllocationCreateInfo { }),
            &image->image,
            &image->allocation,
            nullptr));

        // ---- Pick aspects -----

        switch (format)
        {
        break;case VK_FORMAT_S8_UINT:
            image->aspect = VK_IMAGE_ASPECT_STENCIL_BIT;

        break;case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
            image->aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

        break;case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
            image->aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        break;default:
            image->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        // ---- Make view -----

        if (makeView)
        {
            VkCall(vkCreateImageView(device, Temp(VkImageViewCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = image->image,
                .viewType = viewType,
                .format = format,
                .subresourceRange = { image->aspect, 0, image->mips, 0, image->layers },
            }), nullptr, &image->view));
        }

        return image;
    }

    Image::~Image()
    {
        if (view)
            vkDestroyImageView(context->device, view, nullptr);

        if (allocation)
            vmaDestroyImage(context->vma, image, allocation);
    }

// -----------------------------------------------------------------------------

    void Context::CopyToImage(Image& image, const void* data, size_t size)
    {
        Transition(transferCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        std::memcpy(staging->mapped, data, size);

        vkCmdCopyBufferToImage(transferCmd, staging->buffer, image.image, image.layout, 1, Temp(VkBufferImageCopy {
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageExtent = { image.extent.x, image.extent.y,  1 },
        }));

        transferCommands->Submit(transferCmd, nullptr, semaphore.Raw());
        semaphore->Wait();
        transferCommands->Clear();
        transferCmd = transferCommands->Allocate();
    }

    void Context::GenerateMips(Image& image)
    {
        if (image.mips == 1)
            return;

        Transition(transferCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        int32_t mipWidth = image.extent.x;
        int32_t mipHeight = image.extent.y;

        for (uint32_t mip = 1; mip < image.mips; ++mip)
        {
            vkCmdPipelineBarrier2(transferCmd, Temp(VkDependencyInfo {
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
                    .image = image.image,
                    .subresourceRange = { image.aspect, mip - 1, 1, 0, 1 },
                }),
            }));

            vkCmdBlitImage(transferCmd,
                image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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

        vkCmdPipelineBarrier2(transferCmd, Temp(VkDependencyInfo {
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
                .image = image.image,
                .subresourceRange = { image.aspect, image.mips - 1, 1, 0, 1 },
            }),
        }));

        image.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        transferCommands->Submit(transferCmd, nullptr, semaphore.Raw());
        semaphore->Wait();
        transferCommands->Clear();
        transferCmd = transferCommands->Allocate();
    }

    void Context::Transition(VkCommandBuffer _cmd, Image& image, VkImageLayout newLayout)
    {
        if (image.layout == newLayout)
            return;

        vkCmdPipelineBarrier2(_cmd, Temp(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = Temp(VkImageMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                // TODO: Standard transition sets
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = 0,

                .oldLayout = image.layout,
                .newLayout = newLayout,
                .image = image.image,
                .subresourceRange = { image.aspect, 0, image.mips, 0, image.layers },
            })
        }));

        image.layout = newLayout;
    }

    void Context::TransitionMip(VkCommandBuffer _cmd, Image& image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mip)
    {
        vkCmdPipelineBarrier2(_cmd, Temp(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = Temp(VkImageMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                // TODO: Standard transition sets
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = 0,

                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .image = image.image,
                .subresourceRange = { image.aspect, mip, 1, 0, 1 },
            })
        }));
    }
}