#include "nova_RHI_Impl.hpp"

namespace nova
{
    NOVA_DEFINE_HANDLE_OPERATIONS(Texture)

    Texture::Texture(Context context, Vec3U size, TextureUsage _usage, Format _format, TextureFlags flags)
        : ImplHandle(new TextureImpl)
    {
        impl->context = context;
        impl->format = VkFormat(_format);

        impl->usage = VkImageUsageFlags(_usage);
        bool makeView = (impl->usage & ~(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0;
        impl->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        impl->mips = flags >= TextureFlags::Mips
            ? 1 + u32(std::log2(f32(std::max(size.x, size.y))))
            : 1;

        VkImageType imageType;
        VkImageViewType viewType;
        impl->layers = 1;

        if (flags >= TextureFlags::Array)
        {
            if (size.z > 0)
            {
                impl->layers = size.z;
                size.z = 1;
                imageType = VK_IMAGE_TYPE_2D;
                viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            }
            else
            if (size.y > 0)
            {
                impl->layers = size.y;
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

        impl->extent = glm::max(size, Vec3U(1));

        // ---- Create image -----

        VkCall(vmaCreateImage(context->vma,
            Temp(VkImageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = impl->format,
                .extent = { impl->extent.x, impl->extent.y, impl->extent.z },
                .mipLevels = impl->mips,
                .arrayLayers = impl->layers,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = impl->usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = std::array {
                    context->graphics->family,
                }.data(),
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            }),
            Temp(VmaAllocationCreateInfo { }),
            &impl->image,
            &impl->allocation,
            nullptr));

        // ---- Pick aspects -----

        switch (impl->format)
        {
        break;case VK_FORMAT_S8_UINT:
            impl->aspect = VK_IMAGE_ASPECT_STENCIL_BIT;

        break;case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
            impl->aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

        break;case VK_FORMAT_D16_UNORM_S8_UINT:
              case VK_FORMAT_D24_UNORM_S8_UINT:
              case VK_FORMAT_D32_SFLOAT_S8_UINT:
            impl->aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        break;default:
            impl->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        // ---- Make view -----

        if (makeView)
        {
            VkCall(vkCreateImageView(context->device, Temp(VkImageViewCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = impl->image,
                .viewType = viewType,
                .format = impl->format,
                .subresourceRange = { impl->aspect, 0, impl->mips, 0, impl->layers },
            }), context->pAlloc, &impl->view));
        }
    }

    TextureImpl::~TextureImpl()
    {
        if (view)
            vkDestroyImageView(context->device, view, context->pAlloc);

        if (allocation)
            vmaDestroyImage(context->vma, image, allocation);
    }

// -----------------------------------------------------------------------------

    Vec3U Texture::GetExtent() const noexcept
    {
        return impl->extent;
    }

    Format Texture::GetFormat() const noexcept
    {
        return Format(impl->format);
    }

// -----------------------------------------------------------------------------

    void CommandList::CopyToTexture(Texture dst, Buffer src, u64 srcOffset) const
    {
        Transition(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

        vkCmdCopyBufferToImage(impl->buffer, src->buffer, dst->image, impl->state->Get(dst).layout, 1, Temp(VkBufferImageCopy {
            .bufferOffset = srcOffset,
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageExtent = { dst->extent.x, dst->extent.y,  1 },
        }));
    }

    void CommandList::GenerateMips(Texture texture) const
    {
        if (texture->mips == 1)
            return;

        Transition(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT);

        auto& state = impl->state->Get(texture);

        int32_t mipWidth = texture->extent.x;
        int32_t mipHeight = texture->extent.y;

        for (uint32_t mip = 1; mip < texture->mips; ++mip)
        {
            vkCmdPipelineBarrier2(impl->buffer, Temp(VkDependencyInfo {
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
                    .image = texture->image,
                    .subresourceRange = { texture->aspect, mip - 1, 1, 0, 1 },
                }),
            }));

            vkCmdBlitImage(impl->buffer,
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

        vkCmdPipelineBarrier2(impl->buffer, Temp(VkDependencyInfo {
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

    void CommandList::Transition(Texture texture, VkImageLayout newLayout, VkPipelineStageFlags2 newStages, VkAccessFlags2 newAccess) const
    {
        auto& state = impl->state->Get(texture);

        if (state.layout == newLayout && state.stage == newStages && state.access == newAccess)
            return;

        // TODO: Don't flush full pipeline

        vkCmdPipelineBarrier2(impl->buffer, Temp(VkDependencyInfo {
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
                .image = texture->image,
                .subresourceRange = { texture->aspect, 0, texture->mips, 0, texture->layers },
            })
        }));

        state.layout = newLayout;
        state.stage = newStages;
        state.access = newAccess;
    }

    void CommandList::Transition(Texture texture, ResourceState state, PipelineStage stages) const
    {
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags2 vkStages = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2 access = VkAccessFlags2(0);

        switch (state)
        {
        break;case ResourceState::Sampled:
            layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        break;case ResourceState::ColorAttachment:
            layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        break;case ResourceState::DepthStencilAttachment:
            layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

        break;case ResourceState::Present:
            layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        break;case ResourceState::GeneralImage:
            layout = VK_IMAGE_LAYOUT_GENERAL;
        }

        switch (stages)
        {
        break;case PipelineStage::Graphics:
            // TODO: Support graphics image reads outside of fragment shader
            switch (state)
            {
            break;case ResourceState::Sampled:
                vkStages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

            break;case ResourceState::ColorAttachment:
                vkStages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

            break;case ResourceState::DepthStencilAttachment:
                vkStages = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

            break;case ResourceState::GeneralImage:
                vkStages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            }
        break;case PipelineStage::Compute:
            switch (state)
            {
            break;case ResourceState::Sampled:
                vkStages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

            break;case ResourceState::GeneralImage:
                vkStages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            }
        break;case PipelineStage::RayTracing:
            switch (state)
            {
            break;case ResourceState::Sampled:
                vkStages = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
                access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

            break;case ResourceState::GeneralImage:
                vkStages = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
                access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            }
        }

        Transition(texture, layout, vkStages, access);
    }

    void CommandList::BlitImage(Texture dst, Texture src, Filter filter) const
    {
        Transition(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
        Transition(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

        vkCmdBlitImage(impl->buffer,
            src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, nova::Temp(VkImageBlit {
                .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .srcOffsets = { VkOffset3D{}, VkOffset3D{i32(src.GetExtent().x), i32(src.GetExtent().y), 1} },
                .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .dstOffsets = { VkOffset3D{}, VkOffset3D{i32(dst.GetExtent().x), i32(dst.GetExtent().y), 1} },
            }),
            VkFilter(filter));
    }

// -----------------------------------------------------------------------------

    void CommandList::Clear(Texture texture, Vec4 color) const
    {
        Transition(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_CLEAR_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT);

        vkCmdClearColorImage(impl->buffer,
            texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            nova::Temp(VkClearColorValue {{ color.r, color.g, color.b, color.a }}),
            1, nova::Temp(VkImageSubresourceRange { VK_IMAGE_ASPECT_COLOR_BIT, 0, texture->mips, 0, texture->layers }));
    }
}