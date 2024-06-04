#include "nova_VulkanRHI.hpp"

namespace nova
{
    static
    VkBufferUsageFlags GetBufferUsageFlags(BufferUsage usage, BufferFlags flags)
    {
        auto vk_usage = GetVulkanBufferUsage(usage);
        vk_usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (flags >= BufferFlags::Addressable) {
            vk_usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        }
        return vk_usage;
    }

    // TODO: Clean up host importing, use BufferInfo create struct instead of param list?
    // TODO: Remove resizing?

    Buffer Buffer::Create(u64 size, BufferUsage usage, BufferFlags flags, void* to_import)
    {
        auto context = rhi::Get();

        auto impl = new Impl;
        impl->flags = flags;
        impl->usage = usage;

        Buffer buffer{ impl };
        if (flags >= BufferFlags::ImportHost) {
            // Log("Importing memory");

            constexpr auto MemoryHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;

            vkh::Check(context->vkCreateBuffer(context->device, PtrTo(VkBufferCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                    .pNext = PtrTo(VkExternalMemoryBufferCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
                        .handleTypes = MemoryHandleType,
                    }),
                    .size = size,
                    .usage = GetBufferUsageFlags(usage, flags),
                    .sharingMode = VK_SHARING_MODE_CONCURRENT,
                    .queueFamilyIndexCount = context->queue_family_count,
                    .pQueueFamilyIndices = context->queue_families.data(),
                }), context->alloc, &buffer->buffer));

            VkMemoryRequirements mem_reqs = {};
            context->vkGetBufferMemoryRequirements(context->device, buffer->buffer, &mem_reqs);

            // Log("  Min alignment = {}", context->properties.min_imported_host_pointer_alignment);

            VkMemoryHostPointerPropertiesEXT props = {};
            props.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
            context->vkGetMemoryHostPointerPropertiesEXT(context->device, MemoryHandleType, to_import, &props);

            auto memory_type_bits = mem_reqs.memoryTypeBits & props.memoryTypeBits;

            std::optional<u32> memory_type;
            for (u32 i = 0; i < context->memory_properties.memoryTypeCount; ++i) {
                u32 flag_bit = 1 << i;
                if (memory_type_bits & flag_bit) {
                    // Log("  - can import as type {}", i);
                    memory_type = i;
                }
            }

            auto size_rounded = AlignUpPower2(mem_reqs.size, context->properties.min_imported_host_pointer_alignment);
            vkh::Check(context->vkAllocateMemory(context->device, PtrTo(VkMemoryAllocateInfo {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                    .pNext = PtrTo(VkImportMemoryHostPointerInfoEXT {
                        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT,
                        .handleType = MemoryHandleType,
                        .pHostPointer = to_import,
                    }),
                    .allocationSize = size_rounded,
                    .memoryTypeIndex = memory_type.value(),
                }), context->alloc, &impl->imported));

            vkh::Check(context->vkBindBufferMemory(context->device, buffer->buffer, impl->imported, 0));

            if (impl->flags >= BufferFlags::Addressable) {
                impl->address = context->vkGetBufferDeviceAddress(context->device, PtrTo(VkBufferDeviceAddressInfo {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                    .buffer = impl->buffer,
                }));
            }

            if (impl->flags >= BufferFlags::Mapped) {
                vkh::Check(context->vkMapMemory(context->device, impl->imported, 0, size, 0, &impl->host_address));
            }
        } else {
            buffer.Resize(size);
        }
        return buffer;
    }

    static
    void ResetBuffer(Context ctx, Buffer buffer)
    {
        if (!buffer->buffer) {
            return;
        }

        rhi::stats::AllocationCount--;
        rhi::stats::MemoryAllocated -= buffer->size;

        vmaDestroyBuffer(ctx->vma, buffer->buffer, buffer->allocation);
        buffer->buffer = nullptr;
    }

    void Buffer::Destroy()
    {
        auto context = rhi::Get();

        if (!impl) {
            return;
        }

        if (impl->flags >= BufferFlags::ImportHost) {
            context->vkFreeMemory(context->device, impl->imported, context->alloc);
            context->vkDestroyBuffer(context->device, impl->buffer, context->alloc);
        } else {
            ResetBuffer(context, *this);
        }

        delete impl;
        impl = nullptr;
    }

    void Buffer::Resize(u64 _size) const
    {
        auto context = rhi::Get();

        NOVA_ASSERT(!(impl->flags >= BufferFlags::ImportHost), "Can't resize imported host buffer");

        if (impl->size >= _size) {
            return;
        }

        ResetBuffer(context, *this);
        impl->size = _size;
        rhi::stats::AllocationCount++;
        rhi::stats::NewAllocationCount++;
        rhi::stats::MemoryAllocated += impl->size;

        VmaAllocationCreateFlags vmaFlags = {};
        if (impl->flags >= BufferFlags::Mapped) {
            vmaFlags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
            if (impl->flags >= BufferFlags::DeviceLocal) {
                vmaFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            } else {
                vmaFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            }
        }

        VmaAllocationInfo info;

        vkh::Check(vmaCreateBuffer(
            context->vma,
            PtrTo(VkBufferCreateInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = impl->size,
                .usage = GetBufferUsageFlags(impl->usage, impl->flags),
                .sharingMode = VK_SHARING_MODE_CONCURRENT,
                .queueFamilyIndexCount = context->queue_family_count,
                .pQueueFamilyIndices = context->queue_families.data(),
            }),
            PtrTo(VmaAllocationCreateInfo {
                .flags = vmaFlags,
                .usage = VMA_MEMORY_USAGE_AUTO,
                .requiredFlags = (impl->flags >= BufferFlags::DeviceLocal)
                    ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                    : VkMemoryPropertyFlags(0),
            }),
            &impl->buffer,
            &impl->allocation,
            &info));

        if (impl->flags >= BufferFlags::Addressable) {
            impl->address = context->vkGetBufferDeviceAddress(context->device, PtrTo(VkBufferDeviceAddressInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = impl->buffer,
            }));
        }

        impl->host_address = info.pMappedData;
    }

    u64 Buffer::Size() const
    {
        return impl->size;
    }

    b8* Buffer::HostAddress() const
    {
        return reinterpret_cast<b8*>(impl->host_address);
    }

    u64 Buffer::DeviceAddress() const
    {
        return impl->address;
    }

// -----------------------------------------------------------------------------

    void CommandList::UpdateBuffer(HBuffer dst, const void* data, usz size, u64 dst_offset) const
    {
        auto context = rhi::Get();

        context->vkCmdUpdateBuffer(impl->buffer, dst->buffer, dst_offset, size, data);
    }

    void CommandList::CopyToBuffer(HBuffer dst, HBuffer src, u64 size, u64 dst_offset, u64 src_offset) const
    {
        auto context = rhi::Get();

        context->vkCmdCopyBuffer2(impl->buffer, PtrTo(VkCopyBufferInfo2 {
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .srcBuffer = src->buffer,
            .dstBuffer = dst->buffer,
            .regionCount = 1,
            .pRegions = PtrTo(VkBufferCopy2 {
                .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .srcOffset = src_offset,
                .dstOffset = dst_offset,
                .size = size,
            }),
        }));
    }

    void CommandList::Barrier(HBuffer _buffer, PipelineStage src, PipelineStage dst) const
    {
        auto context = rhi::Get();

        context->vkCmdPipelineBarrier2(impl->buffer, PtrTo(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = PtrTo(VkBufferMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask = GetVulkanPipelineStage(src),
                .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = GetVulkanPipelineStage(dst),
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .buffer = _buffer->buffer,
                .size = VK_WHOLE_SIZE,
            }),
        }));
    }
}