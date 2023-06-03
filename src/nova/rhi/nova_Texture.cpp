#include "nova_RHI.hpp"

namespace nova
{
    Texture::Texture(Context& _context, Vec3U size, TextureUsage _usage, Format _format, TextureFlags flags)
        : context(&_context)
        , format(VkFormat(_format))
    {
        auto usage = VkImageUsageFlags(_usage);
        bool makeView = (usage & ~(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        mips = flags >= TextureFlags::Mips
            ? 1 + u32(std::log2(f32(std::max(size.x, size.y))))
            : 1;

        VkImageType imageType;
        VkImageViewType viewType;
        layers = 1;

        if (flags >= TextureFlags::Array)
        {
            if (size.z > 0)
            {
                layers = size.z;
                size.z = 1;
                imageType = VK_IMAGE_TYPE_2D;
                viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            }
            else
            if (size.y > 0)
            {
                layers = size.y;
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

        extent = glm::max(size, Vec3U(1));

        // ---- Create image -----

        VkCall(vmaCreateImage(context->vma,
            Temp(VkImageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = format,
                .extent = { extent.x, extent.y, extent.z },
                .mipLevels = mips,
                .arrayLayers = layers,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = std::array {
                    context->graphics.family,
                }.data(),
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            }),
            Temp(VmaAllocationCreateInfo { }),
            &image,
            &allocation,
            nullptr));

        // ---- Pick aspects -----

        switch (format)
        {
        break;case VK_FORMAT_S8_UINT:
            aspect = VK_IMAGE_ASPECT_STENCIL_BIT;

        break;case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
            aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

        break;case VK_FORMAT_D16_UNORM_S8_UINT:
              case VK_FORMAT_D24_UNORM_S8_UINT:
              case VK_FORMAT_D32_SFLOAT_S8_UINT:
            aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        break;default:
            aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        // ---- Make view -----

        if (makeView)
        {
            VkCall(vkCreateImageView(context->device, Temp(VkImageViewCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = image,
                .viewType = viewType,
                .format = format,
                .subresourceRange = { aspect, 0, mips, 0, layers },
            }), context->pAlloc, &view));
        }
    }

    Texture::~Texture()
    {
        if (view)
            vkDestroyImageView(context->device, view, context->pAlloc);

        if (allocation)
            vmaDestroyImage(context->vma, image, allocation);
    }

    Texture::Texture(Texture&& other) noexcept
        : context(other.context)
        , image(other.image)
        , allocation(other.allocation)
        , view(other.view)
        , format(other.format)
        , aspect(other.aspect)
        , extent(other.extent)
        , mips(other.mips)
        , layers(other.layers)
    {
        other.image = nullptr;
        other.view = nullptr;
        other.allocation = nullptr;
    }

// -----------------------------------------------------------------------------

    void CommandList::CopyToTexture(Texture& dst, Buffer& src, u64 srcOffset)
    {
        Transition(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT);

        vkCmdCopyBufferToImage(buffer, src.buffer, dst.image, tracker->Get(dst).layout, 1, Temp(VkBufferImageCopy {
            .bufferOffset = srcOffset,
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageExtent = { dst.extent.x, dst.extent.y,  1 },
        }));
    }

    void CommandList::GenerateMips(Texture& texture)
    {
        if (texture.mips == 1)
            return;

        Transition(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT);

        auto& state = tracker->Get(texture);

        int32_t mipWidth = texture.extent.x;
        int32_t mipHeight = texture.extent.y;

        for (uint32_t mip = 1; mip < texture.mips; ++mip)
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
                    .image = texture.image,
                    .subresourceRange = { texture.aspect, mip - 1, 1, 0, 1 },
                }),
            }));

            vkCmdBlitImage(buffer,
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
                .image = texture.image,
                .subresourceRange = { texture.aspect, texture.mips - 1, 1, 0, 1 },
            }),
        }));

        state.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    void CommandList::Transition(Texture& texture, VkImageLayout newLayout, VkPipelineStageFlags2 newStages, VkAccessFlags2 newAccess)
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
                .image = texture.image,
                .subresourceRange = { texture.aspect, 0, texture.mips, 0, texture.layers },
            })
        }));

        state.layout = newLayout;
        state.stage = newStages;
        state.access = newAccess;
    }

    void CommandList::Transition(Texture& texture, ResourceState state, BindPoint bindPoint)
    {
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2 access = VkAccessFlags2(0);

        bool set = false;

        switch (state)
        {
        break;case ResourceState::GeneralImage:
            layout = VK_IMAGE_LAYOUT_GENERAL;
            access = VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT;
            set = true;
        break;case ResourceState::Present:
            layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            stages = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            set = true;
        }

        switch (bindPoint)
        {
        break;case BindPoint::Compute:
            stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            set = true;
        break;case BindPoint::Graphics:
            stages = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
            set = true;
        break;case BindPoint::RayTracing:
            stages = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
            set = true;
        }

        if (set)
        {
            Transition(texture, layout, stages, access);
        }
        else
        {
            NOVA_THROW("Unknown transition ({}, {})", u32(state), u32(bindPoint));
        }
    }

// -----------------------------------------------------------------------------

    void CommandList::Clear(Texture& texture, Vec4 color)
    {
        Transition(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_CLEAR_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT);

        vkCmdClearColorImage(buffer,
            texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            nova::Temp(VkClearColorValue {{ color.r, color.g, color.b, color.a }}),
            1, nova::Temp(VkImageSubresourceRange { VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.mips, 0, texture.layers }));
    }
}