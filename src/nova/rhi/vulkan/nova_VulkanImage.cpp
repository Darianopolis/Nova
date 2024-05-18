#include "nova_VulkanRHI.hpp"

namespace nova
{
    Sampler Sampler::Create(HContext context, Filter filter, AddressMode address_mode, BorderColor color, f32 anisotropy)
    {
        auto impl = new Impl;
        impl->context = context;

        vkh::Check(impl->context->vkCreateSampler(context->device, PtrTo(VkSamplerCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = GetVulkanFilter(filter),
            .minFilter = GetVulkanFilter(filter),
            // TODO: Separate option
            .mipmapMode = filter == Filter::Linear
                ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                : VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = GetVulkanAddressMode(address_mode),
            .addressModeV = GetVulkanAddressMode(address_mode),
            .addressModeW = GetVulkanAddressMode(address_mode),
            .anisotropyEnable = anisotropy > 0.f,
            .maxAnisotropy = anisotropy,
            .maxLod = VK_LOD_CLAMP_NONE,
            .borderColor = GetVulkanBorderColor(color),
        }), context->alloc, &impl->sampler));

        {
            std::scoped_lock lock{ context->global_heap.mutex };
            impl->descriptor_index = context->global_heap.sampler_handles.Acquire();
#ifdef NOVA_RHI_NOISY_ALLOCATIONS
            Log("Sampler Descriptor Acquired: {}", impl->descriptorIndex);
#endif
        }
        context->global_heap.WriteSampler(impl->descriptor_index, impl);

        return { impl };
    }

    void Sampler::Destroy()
    {
        if (!impl) {
            return;
        }

        impl->context->vkDestroySampler(impl->context->device, impl->sampler, impl->context->alloc);

        {
            std::scoped_lock lock{ impl->context->global_heap.mutex };
            impl->context->global_heap.sampler_handles.Release(impl->descriptor_index);
#ifdef NOVA_RHI_NOISY_ALLOCATIONS
            Log("Sampler Descriptor Released: {}", impl->descriptorIndex);
#endif
        }

        delete impl;
        impl = nullptr;
    }

    u32 Sampler::Descriptor() const
    {
        return impl->descriptor_index;
    }

// -----------------------------------------------------------------------------

    Image Image::Create(HContext context, Vec3U size, ImageUsage usage, nova::Format format, ImageFlags flags)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->format = format;
        impl->usage = usage;
        bool make_view = (GetVulkanImageUsage(usage) & ~(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0;

        impl->mips = flags >= ImageFlags::Mips
            ? 1 + u32(std::log2(f32(std::max(size.x, size.y))))
            : 1;

        VkImageType image_type;
        VkImageViewType view_type;
        impl->layers = 1;

        if (flags >= ImageFlags::Array) {
            if (size.z > 0) {
                impl->layers = size.z;
                size.z = 1;
                image_type = VK_IMAGE_TYPE_2D;
                view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

            } else if (size.y > 0) {
                impl->layers = size.y;
                size.y = 1;
                image_type = VK_IMAGE_TYPE_1D;
                view_type = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            } else {
                NOVA_THROW("Image array must have at least 1 dimension and 1 layer");
            }
        } else {
            if (size.z > 0) {
                image_type = VK_IMAGE_TYPE_3D;
                view_type = VK_IMAGE_VIEW_TYPE_3D;

            } else if (size.y > 0) {
                image_type = VK_IMAGE_TYPE_2D;
                view_type = VK_IMAGE_VIEW_TYPE_2D;

            } else if (size.z > 0) {
                image_type = VK_IMAGE_TYPE_1D;
                view_type = VK_IMAGE_VIEW_TYPE_1D;
            } else {
                NOVA_THROW("Image must have at least one non-zero dimension");
            }
        }

        impl->extent = glm::max(size, Vec3U(1));

        // ---- Create image -----

        VmaAllocationInfo info;

        VkMemoryPropertyFlags vk_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        auto vk_usage = GetVulkanImageUsage(impl->usage);
        vk_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (!context->transfer_manager.staged_image_copy) {
            vk_usage |= VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT;
            vk_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        }

        vkh::Check(vmaCreateImage(context->vma,
            PtrTo(VkImageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = GetVulkanFormat(impl->format).vk_format,
                .extent = { impl->extent.x, impl->extent.y, impl->extent.z },
                .mipLevels = impl->mips,
                .arrayLayers = impl->layers,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = vk_usage,
                .sharingMode = VK_SHARING_MODE_CONCURRENT,
                .queueFamilyIndexCount = context->queue_family_count,
                .pQueueFamilyIndices = context->queue_families.data(),
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            }),
            PtrTo(VmaAllocationCreateInfo {
                .usage = VMA_MEMORY_USAGE_AUTO,
                .requiredFlags = vk_flags,
            }),
            &impl->image,
            &impl->allocation,
            &info));

        rhi::stats::MemoryAllocated += info.size;

        // ---- Pick aspects -----

        switch (GetVulkanFormat(impl->format).vk_format) {
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

        if (make_view) {
            vkh::Check(impl->context->vkCreateImageView(context->device, PtrTo(VkImageViewCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = impl->image,
                .viewType = view_type,
                .format = GetVulkanFormat(impl->format).vk_format,
                .subresourceRange = { impl->aspect, 0, impl->mips, 0, impl->layers },
            }), context->alloc, &impl->view));
        }

        return { impl };
    }

    void Image::Destroy()
    {
        if (!impl) {
            return;
        }

        if (impl->view) {
            impl->context->vkDestroyImageView(impl->context->device, impl->view, impl->context->alloc);
        }

        if (impl->allocation) {
            VmaAllocationInfo info;
            vmaGetAllocationInfo(impl->context->vma, impl->allocation, &info);
            rhi::stats::MemoryAllocated -= info.size;
            vmaDestroyImage(impl->context->vma, impl->image, impl->allocation);
        }

        if (impl->descriptor_index != UINT_MAX) {
            auto& heap = impl->context->global_heap;
            std::scoped_lock lock{ heap.mutex };
            impl->context->global_heap.image_handles.Release(impl->descriptor_index);
#ifdef NOVA_RHI_NOISY_ALLOCATIONS
            Log("Image Descriptor Released: {} (total = {})",
                impl->descriptorIndex, heap.imageHandles.nextIndex - heap.imageHandles.freeList.size());
#endif
        }

        delete impl;
        impl = nullptr;
    }

    u32 Image::Descriptor() const
    {
        if (impl->descriptor_index == UINT_MAX) {
            auto& heap = impl->context->global_heap;
            {
                std::scoped_lock lock{ heap.mutex };
                impl->descriptor_index = heap.image_handles.Acquire();
#ifdef NOVA_RHI_NOISY_ALLOCATIONS
                Log("Image Descriptor Acquired: {} (total = {})",
                    impl->descriptorIndex, heap.imageHandles.nextIndex - heap.imageHandles.freeList.size());
#endif
            }
            if (impl->usage >= nova::ImageUsage::Sampled) {
                heap.WriteSampled(impl->descriptor_index, *this);
            }
            if (impl->usage >= nova::ImageUsage::Storage) {
                heap.WriteStorage(impl->descriptor_index, *this);
            }
        }

        return impl->descriptor_index;
    }

    Vec3U Image::Extent() const
    {
        return impl->extent;
    }

    Format Image::Format() const
    {
        return impl->format;
    }

    void Image::Set(Vec3I offset, Vec3U extent, const void* data) const
    {
        // TODO: Layers
        // TODO: Mips

        if (impl->context->transfer_manager.staged_image_copy) {
            auto& manager = impl->context->transfer_manager;
            std::scoped_lock lock{ manager.mutex };

            auto format = GetVulkanFormat(impl->format);

            usz rows = (extent.y + format.block_height - 1) / format.block_height;
            usz cols = (extent.x + format.block_width  - 1) / format.block_width;
            usz size = rows * cols * format.atom_size;

            manager.staging.Set(Span<char>((char*)data, size));
            auto cmd = manager.queue.Begin();
            cmd.CopyToImage(*this, manager.staging);
            manager.queue.Submit({cmd}, {}).Wait();
        } else {
            impl->context->vkTransitionImageLayoutEXT(impl->context->device, 1, PtrTo(VkHostImageLayoutTransitionInfoEXT {
                .sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT,
                .image = impl->image,
                .oldLayout = impl->layout,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .subresourceRange{ impl->aspect, 0, impl->mips, 0, impl->layers },
            }));

            impl->context->vkCopyMemoryToImageEXT(impl->context->device, PtrTo(VkCopyMemoryToImageInfoEXT {
                .sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT,
                .dstImage = impl->image,
                .dstImageLayout = VK_IMAGE_LAYOUT_GENERAL,
                .regionCount = 1,
                .pRegions = PtrTo(VkMemoryToImageCopyEXT {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT,
                    .pHostPointer = data,
                    .imageSubresource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                    .imageOffset{ offset.x, offset.y, offset.z },
                    .imageExtent{ extent.x, extent.y, extent.z },
                })
            }));

            impl->layout = VK_IMAGE_LAYOUT_GENERAL;
            impl->stage = VK_PIPELINE_STAGE_2_HOST_BIT;
        }
    }

    void Image::Transition(ImageLayout layout) const
    {
        if (impl->context->transfer_manager.staged_image_copy) {
            auto& manager = impl->context->transfer_manager;
            std::scoped_lock lock{ manager.mutex };
            auto cmd = manager.queue.Begin();
            cmd.Transition(*this, layout, nova::PipelineStage::All);
            manager.queue.Submit({cmd}, {}).Wait();
        } else {
            impl->context->vkTransitionImageLayoutEXT(impl->context->device, 1, PtrTo(VkHostImageLayoutTransitionInfoEXT {
                .sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT,
                .image = impl->image,
                .oldLayout = impl->layout,
                .newLayout = GetVulkanImageLayout(layout),
                .subresourceRange{ impl->aspect, 0, impl->mips, 0, impl->layers },
            }));

            impl->layout = GetVulkanImageLayout(layout);
            impl->stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        }
    }

// -----------------------------------------------------------------------------

    void CommandList::Impl::Transition(HImage image,
        VkImageLayout new_layout, VkPipelineStageFlags2 new_stages)
    {
        context->vkCmdPipelineBarrier2(buffer, PtrTo(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = PtrTo(VkImageMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                .srcStageMask = image->stage,
                .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = new_stages,
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,

                .oldLayout = image->layout,
                .newLayout = new_layout,
                .image = image->image,
                .subresourceRange{ image->aspect, 0, image->mips, 0, image->layers },
            })
        }));

        image->layout = new_layout;
        image->stage = new_stages;
    }

    void CommandList::Transition(HImage image, ImageLayout layout, PipelineStage stage) const
    {
        auto queue = impl->queue;
        auto vk_layout = GetVulkanImageLayout(layout);
        auto vk_stage = GetVulkanPipelineStage(stage) & queue->stages;

        impl->Transition(image, vk_layout, vk_stage);
    }

    void CommandList::ClearColor(HImage image, std::variant<Vec4, Vec4U, Vec4I> value) const
    {
        impl->Transition(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_CLEAR_BIT);

        impl->context->vkCmdClearColorImage(impl->buffer,
            image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            nova::PtrTo(std::visit(Overloads {
                [&](const Vec4&  v) { return VkClearColorValue{ .float32{ v.r, v.g, v.b, v.a }}; },
                [&](const Vec4U& v) { return VkClearColorValue{  .uint32{ v.r, v.g, v.b, v.a }}; },
                [&](const Vec4I& v) { return VkClearColorValue{   .int32{ v.r, v.g, v.b, v.a }}; },
            }, value)),
            1, nova::PtrTo(VkImageSubresourceRange {
                VK_IMAGE_ASPECT_COLOR_BIT, 0, image->mips, 0, image->layers }));
    }

    void CommandList::CopyToImage(HImage dst, HBuffer src, u64 src_offset) const
    {
        impl->Transition(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_COPY_BIT);

        impl->context->vkCmdCopyBufferToImage2(impl->buffer, PtrTo(VkCopyBufferToImageInfo2 {
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
            .srcBuffer = src->buffer,
            .dstImage = dst->image,
            .dstImageLayout = dst->layout,
            .regionCount = 1,
            .pRegions = PtrTo(VkBufferImageCopy2 {
                .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                .bufferOffset = src_offset,
                .imageSubresource{ dst->aspect, 0, 0, dst->layers },
                .imageExtent{ dst->extent.x, dst->extent.y,  1 },
            }),
        }));
    }

    void CommandList::CopyToImage(HImage dst, HImage src) const
    {
        impl->Transition(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT);
        impl->Transition(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT);

        impl->context->vkCmdCopyImage(impl->buffer,
            src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, PtrTo(VkImageCopy {
                .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .srcOffset = {},
                .dstSubresource ={ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .dstOffset = {},
                .extent = VkExtent3D(dst->extent.x, dst->extent.y, dst->extent.z),
            }));
    }

    void CommandList::CopyFromImage(HBuffer dst, HImage src, Rect2D region) const
    {
        impl->Transition(src,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT);
        impl->context->vkCmdCopyImageToBuffer2(impl->buffer, nova::PtrTo(VkCopyImageToBufferInfo2 {
            .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
            .srcImage = src->image,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstBuffer = dst->buffer,
            .regionCount = 1,
            .pRegions = nova::PtrTo(VkBufferImageCopy2 {
                .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                .imageSubresource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .imageOffset{ region.offset.x, region.offset.y, 0 },
                .imageExtent{ region.extent.x, region.extent.y, 1 },
            }),
        }));
    }

    void CommandList::GenerateMips(HImage image) const
    {
        if (image->mips == 1) {
            return;
        }

        impl->Transition(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);

        int32_t mip_width = image->extent.x;
        int32_t mip_height = image->extent.y;

        for (uint32_t mip = 1; mip < image->mips; ++mip) {
            impl->context->vkCmdPipelineBarrier2(impl->buffer, PtrTo(VkDependencyInfo {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = PtrTo(VkImageMemoryBarrier2 {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                    .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                    .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                    .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT,

                    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .image = image->image,
                    .subresourceRange = { image->aspect, mip - 1, 1, 0, 1 },
                }),
            }));

            impl->context->vkCmdBlitImage(impl->buffer,
                image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, PtrTo(VkImageBlit {
                    .srcSubresource{ VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 0, 1 },
                    .srcOffsets{ VkOffset3D{}, VkOffset3D{(int32_t)mip_width, (int32_t)mip_height, 1} },
                    .dstSubresource{ VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, 1 },
                    .dstOffsets{ VkOffset3D{}, VkOffset3D{(int32_t)std::max(mip_width / 2, 1), (int32_t)std::max(mip_height / 2, 1), 1} },
                }),
                VK_FILTER_LINEAR);

            mip_width = std::max(mip_width / 2, 1);
            mip_height = std::max(mip_height / 2, 1);
        }

        impl->context->vkCmdPipelineBarrier2(impl->buffer, PtrTo(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = PtrTo(VkImageMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,

                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .image = image->image,
                .subresourceRange{ image->aspect, image->mips - 1, 1, 0, 1 },
            }),
        }));

        image->layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    void CommandList::BlitImage(HImage dst, HImage src, Filter filter) const
    {
        impl->Transition(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT);
        impl->Transition(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT);

        impl->context->vkCmdBlitImage(impl->buffer,
            src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, nova::PtrTo(VkImageBlit {
                .srcSubresource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .srcOffsets{ VkOffset3D{}, VkOffset3D{i32(src->extent.x), i32(src->extent.y), 1} },
                .dstSubresource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .dstOffsets{ VkOffset3D{}, VkOffset3D{i32(dst->extent.x), i32(dst->extent.y), 1} },
            }),
            GetVulkanFilter(filter));
    }
}