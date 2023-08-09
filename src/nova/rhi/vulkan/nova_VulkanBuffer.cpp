#include "nova_VulkanRHI.hpp"

namespace nova
{
    HBuffer Buffer::Create(HContext context, u64 size, BufferUsage usage, BufferFlags flags)
    {
        auto impl = new Buffer;
        impl->context = context;
        impl->flags = flags;
        impl->usage = usage;

        impl->Resize(size);

        return impl;
    }

    static
    void ResetBuffer(HContext ctx, HBuffer buffer)
    {
        if (!buffer->buffer)
            return;

        vmaDestroyBuffer(ctx->vma, buffer->buffer, buffer->allocation);
        buffer = nullptr;
    }

    Buffer::~Buffer()
    {
        ResetBuffer(context, this);
    }

    void Buffer::Resize(u64 _size)
    {
        if (size >= _size)
            return;

        size = _size;

        ResetBuffer(context, this);

        VmaAllocationCreateFlags vmaFlags = {};
        if (flags >= BufferFlags::Mapped)
        {
            vmaFlags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
            if (flags >= BufferFlags::DeviceLocal)
                vmaFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            else
                vmaFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        }

        auto vkUsage = GetVulkanBufferUsage(usage);
        vkUsage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (flags >= BufferFlags::Addressable)
            vkUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VkCall(vmaCreateBuffer(
            context->vma,
            Temp(VkBufferCreateInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = size,
                .usage = vkUsage,
                .sharingMode = VK_SHARING_MODE_CONCURRENT,
                .queueFamilyIndexCount = 3,
                .pQueueFamilyIndices = std::array {
                    0u, 1u, 2u, // TODO
                }.data(),
            }),
            Temp(VmaAllocationCreateInfo {
                .flags = vmaFlags,
                .usage = VMA_MEMORY_USAGE_AUTO,
                .requiredFlags = (flags >= BufferFlags::DeviceLocal)
                    ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                    : VkMemoryPropertyFlags(0),
            }),
            &buffer,
            &allocation,
            nullptr));

        if (flags >= BufferFlags::Addressable)
        {
            address = vkGetBufferDeviceAddress(context->device, Temp(VkBufferDeviceAddressInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = buffer,
            }));
        }
    }

    u64 Buffer::GetSize()
    {
        return size;
    }

    b8* Buffer::GetMapped()
    {
        VmaAllocationInfo info;
        vmaGetAllocationInfo(context->vma, allocation, &info);
        return reinterpret_cast<b8*>(info.pMappedData);
    }

    u64 Buffer::GetAddress()
    {
        return address;
    }

// -----------------------------------------------------------------------------

    void CommandList::UpdateBuffer(HBuffer dst, const void* pData, usz size, u64 dstOffset)
    {
        vkCmdUpdateBuffer(buffer, dst->buffer, dstOffset, size, pData);
    }

    void CommandList::CopyToBuffer(HBuffer dst, HBuffer src, u64 size, u64 dstOffset, u64 srcOffset)
    {
        vkCmdCopyBuffer2(buffer, Temp(VkCopyBufferInfo2 {
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .srcBuffer = src->buffer,
            .dstBuffer = dst->buffer,
            .regionCount = 1,
            .pRegions = Temp(VkBufferCopy2 {
                .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .srcOffset = srcOffset,
                .dstOffset = dstOffset,
                .size = size,
            }),
        }));
    }

    void CommandList::Barrier(HBuffer _buffer, PipelineStage src, PipelineStage dst)
    {
        vkCmdPipelineBarrier2(buffer, Temp(VkDependencyInfo {
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