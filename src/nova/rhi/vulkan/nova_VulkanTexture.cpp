#include "nova_VulkanRHI.hpp"

namespace nova
{
    Sampler VulkanContext::Sampler_Create(Filter filter, AddressMode addressMode, BorderColor color, f32 anisotropy)
    {
        auto[id, sampler] = samplers.Acquire();

        VkCall(vkCreateSampler(device, Temp(VkSamplerCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VkFilter(filter),
            .minFilter = VkFilter(filter),
            // TODO: Separate option
            .mipmapMode = filter == Filter::Linear
                ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                : VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VkSamplerAddressMode(addressMode),
            .addressModeV = VkSamplerAddressMode(addressMode),
            .addressModeW = VkSamplerAddressMode(addressMode),
            .anisotropyEnable = anisotropy > 0.f,
            .maxAnisotropy = anisotropy,
            .minLod = -1000.f,
            .maxLod = 1000.f,
            .borderColor = VkBorderColor(color),
        }), pAlloc, &sampler.sampler));

        return id;
    }

    void VulkanContext::Sampler_Destroy(Sampler sampler)
    {
        vkDestroySampler(device, Get(sampler).sampler, pAlloc);
        samplers.Return(sampler);
    }

// -----------------------------------------------------------------------------

    Texture VulkanContext::Texture_Create(Vec3U size, TextureUsage usage, Format format, TextureFlags flags)
    {
        auto[id, texture] = textures.Acquire();

        texture.format = VkFormat(format);
        texture.usage = VkImageUsageFlags(usage);
        bool makeView = (texture.usage & ~(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0;
        texture.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        texture.mips = flags >= TextureFlags::Mips
            ? 1 + u32(std::log2(f32(std::max(size.x, size.y))))
            : 1;

        VkImageType imageType;
        VkImageViewType viewType;
        texture.layers = 1;

        if (flags >= TextureFlags::Array)
        {
            if (size.z > 0)
            {
                texture.layers = size.z;
                size.z = 1;
                imageType = VK_IMAGE_TYPE_2D;
                viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            }
            else
            if (size.y > 0)
            {
                texture.layers = size.y;
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

        texture.extent = glm::max(size, Vec3U(1));

        // ---- Create image -----

        VkCall(vmaCreateImage(vma,
            Temp(VkImageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = texture.format,
                .extent = { texture.extent.x, texture.extent.y, texture.extent.z },
                .mipLevels = texture.mips,
                .arrayLayers = texture.layers,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = texture.usage,
                // .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .sharingMode = VK_SHARING_MODE_CONCURRENT,
                .queueFamilyIndexCount = 3,
                .pQueueFamilyIndices = std::array {
                    0u, 1u, 2u, // TODO
                }.data(),
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            }),
            Temp(VmaAllocationCreateInfo { }),
            &texture.image,
            &texture.allocation,
            nullptr));

        // ---- Pick aspects -----

        switch (texture.format)
        {
        break;case VK_FORMAT_S8_UINT:
            texture.aspect = VK_IMAGE_ASPECT_STENCIL_BIT;

        break;case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
            texture.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

        break;case VK_FORMAT_D16_UNORM_S8_UINT:
              case VK_FORMAT_D24_UNORM_S8_UINT:
              case VK_FORMAT_D32_SFLOAT_S8_UINT:
            texture.aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        break;default:
            texture.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        // ---- Make view -----

        if (makeView)
        {
            VkCall(vkCreateImageView(device, Temp(VkImageViewCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = texture.image,
                .viewType = viewType,
                .format = texture.format,
                .subresourceRange = { texture.aspect, 0, texture.mips, 0, texture.layers },
            }), pAlloc, &texture.view));
        }

        return id;
    }

    void VulkanContext::Texture_Destroy(Texture id)
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

    void VulkanContext::Cmd_CopyToTexture(CommandList cmd, Texture hDst, Buffer hSrc, u64 srcOffset)
    {
        Cmd_Transition(cmd, hDst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

        auto& dst = Get(hDst);
        auto& src = Get(hSrc);

        vkCmdCopyBufferToImage2(Get(cmd).buffer, Temp(VkCopyBufferToImageInfo2 {
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
            .srcBuffer = src.buffer,
            .dstImage = dst.image,
            .dstImageLayout = Get(Get(cmd).state).imageStates[dst.image].layout,
            .regionCount = 1,
            .pRegions = Temp(VkBufferImageCopy2 {
                .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                .bufferOffset = srcOffset,
                .imageSubresource = { dst.aspect, 0, 0, dst.layers },
                .imageExtent = { dst.extent.x, dst.extent.y,  1 },
            }),
        }));
    }

    void VulkanContext::Cmd_GenerateMips(CommandList cmd, Texture hTexture)
    {
        auto& cmdState = Get(Get(cmd).state);
        auto& texture = Get(hTexture);

        if (texture.mips == 1)
            return;

        Cmd_Transition(cmd, hTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT);

        auto& state = cmdState.imageStates[texture.image];

        int32_t mipWidth = texture.extent.x;
        int32_t mipHeight = texture.extent.y;

        for (uint32_t mip = 1; mip < texture.mips; ++mip)
        {
            vkCmdPipelineBarrier2(Get(cmd).buffer, Temp(VkDependencyInfo {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = Temp(VkImageMemoryBarrier2 {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                    .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                    .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                    .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT,

                    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .image = texture.image,
                    .subresourceRange = { texture.aspect, mip - 1, 1, 0, 1 },
                }),
            }));

            vkCmdBlitImage(Get(cmd).buffer,
                texture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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

        vkCmdPipelineBarrier2(Get(cmd).buffer, Temp(VkDependencyInfo {
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
                .image = texture.image,
                .subresourceRange = { texture.aspect, texture.mips - 1, 1, 0, 1 },
            }),
        }));

        state.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    void VulkanContext::Cmd_BlitImage(CommandList cmd, Texture dst, Texture src, Filter filter)
    {
        Cmd_Transition(cmd, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
        Cmd_Transition(cmd, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

        vkCmdBlitImage(Get(cmd).buffer,
            Get(src).image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            Get(dst).image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, nova::Temp(VkImageBlit {
                .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .srcOffsets = { VkOffset3D{}, VkOffset3D{i32(Texture_GetExtent(src).x), i32(Texture_GetExtent(src).y), 1} },
                .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .dstOffsets = { VkOffset3D{}, VkOffset3D{i32(Texture_GetExtent(dst).x), i32(Texture_GetExtent(dst).y), 1} },
            }),
            VkFilter(filter));
    }
}