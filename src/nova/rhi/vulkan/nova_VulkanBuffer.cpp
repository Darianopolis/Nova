#include "nova_VulkanRHI.hpp"

namespace nova
{
    Buffer Buffer::Create(HContext context, u64 size, BufferUsage usage, BufferFlags flags)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->flags = flags;
        impl->usage = usage;

        Buffer buffer{ impl };
        buffer.Resize(size);
        return buffer;
    }

    static
    void ResetBuffer(Context ctx, Buffer buffer)
    {
        if (!buffer->buffer) {
            return;
        }

        rhi::stats::MemoryAllocated -= buffer->size;

        vmaDestroyBuffer(ctx->vma, buffer->buffer, buffer->allocation);
        buffer->buffer = nullptr;
    }

    void Buffer::Destroy()
    {
        if (!impl) {
            return;
        }

        ResetBuffer(impl->context, *this);

        delete impl;
        impl = nullptr;
    }

    void Buffer::Resize(u64 _size) const
    {
        if (impl->size >= _size) {
            return;
        }

        ResetBuffer(impl->context, *this);
        impl->size = _size;
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

        auto vk_usage = GetVulkanBufferUsage(impl->usage);
        vk_usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (impl->flags >= BufferFlags::Addressable) {
            vk_usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        }

        vkh::Check(vmaCreateBuffer(
            impl->context->vma,
            Temp(VkBufferCreateInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = impl->size,
                .usage = vk_usage,
                .sharingMode = VK_SHARING_MODE_CONCURRENT,
                .queueFamilyIndexCount = impl->context->queue_family_count,
                .pQueueFamilyIndices = impl->context->queue_families.data(),
            }),
            Temp(VmaAllocationCreateInfo {
                .flags = vmaFlags,
                .usage = VMA_MEMORY_USAGE_AUTO,
                .requiredFlags = (impl->flags >= BufferFlags::DeviceLocal)
                    ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                    : VkMemoryPropertyFlags(0),
            }),
            &impl->buffer,
            &impl->allocation,
            nullptr));

        if (impl->flags >= BufferFlags::Addressable) {
            impl->address = impl->context->vkGetBufferDeviceAddress(impl->context->device, Temp(VkBufferDeviceAddressInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = impl->buffer,
            }));
        }
    }

    u64 Buffer::GetSize() const
    {
        return impl->size;
    }

    b8* Buffer::GetMapped() const
    {
        VmaAllocationInfo info;
        vmaGetAllocationInfo(impl->context->vma, impl->allocation, &info);
        return reinterpret_cast<b8*>(info.pMappedData);
    }

    u64 Buffer::GetAddress() const
    {
        return impl->address;
    }

// -----------------------------------------------------------------------------

    void CommandList::UpdateBuffer(HBuffer dst, const void* data, usz size, u64 dst_offset) const
    {
        impl->context->vkCmdUpdateBuffer(impl->buffer, dst->buffer, dst_offset, size, data);
    }

    void CommandList::CopyToBuffer(HBuffer dst, HBuffer src, u64 size, u64 dst_offset, u64 src_offset) const
    {
        impl->context->vkCmdCopyBuffer2(impl->buffer, Temp(VkCopyBufferInfo2 {
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .srcBuffer = src->buffer,
            .dstBuffer = dst->buffer,
            .regionCount = 1,
            .pRegions = Temp(VkBufferCopy2 {
                .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .srcOffset = src_offset,
                .dstOffset = dst_offset,
                .size = size,
            }),
        }));
    }

    void CommandList::Barrier(HBuffer _buffer, PipelineStage src, PipelineStage dst) const
    {
        impl->context->vkCmdPipelineBarrier2(impl->buffer, Temp(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = Temp(VkBufferMemoryBarrier2 {
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