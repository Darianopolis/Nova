#include "nova_VulkanRHI.hpp"

namespace nova
{
    Buffer VulkanContext::Buffer_Create(u64 size, BufferUsage usage, BufferFlags flags)
    {
        auto[id, buffer] = buffers.Acquire();

        buffer.buffer = nullptr;
        buffer.allocation = nullptr;

        buffer.flags = flags;
        buffer.usage = usage;

        buffer.size = 0;

        Buffer_Resize(id, size);

        return id;
    }

    static
    void ResetBuffer(VulkanContext& ctx, VulkanBuffer& buffer)
    {
        if (!buffer.buffer)
            return;

        vmaDestroyBuffer(ctx.vma, buffer.buffer, buffer.allocation);
    }

    void VulkanContext::Buffer_Destroy(Buffer id)
    {
        ResetBuffer(*this, Get(id));

        buffers.Return(id);
    }

    void VulkanContext::Buffer_Resize(Buffer id, u64 size)
    {
        auto& buffer = Get(id);

        if (buffer.size >= size)
            return;

        buffer.size = size;

        ResetBuffer(*this, buffer);

        VmaAllocationCreateFlags vmaFlags = {};
        if (buffer.flags >= BufferFlags::Mapped)
        {
            vmaFlags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
            if (buffer.flags >= BufferFlags::DeviceLocal)
                vmaFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            else
                vmaFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        }

        auto usage = GetVulkanBufferUsage(buffer.usage);
        usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (buffer.flags >= BufferFlags::Addressable)
            usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VkCall(vmaCreateBuffer(
            vma,
            Temp(VkBufferCreateInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = buffer.size,
                .usage = usage,
                .sharingMode = VK_SHARING_MODE_CONCURRENT,
                .queueFamilyIndexCount = 3,
                .pQueueFamilyIndices = std::array {
                    0u, 1u, 2u, // TODO
                }.data(),
            }),
            Temp(VmaAllocationCreateInfo {
                .flags = vmaFlags,
                .usage = VMA_MEMORY_USAGE_AUTO,
                .requiredFlags = (buffer.flags >= BufferFlags::DeviceLocal)
                    ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                    : VkMemoryPropertyFlags(0),
            }),
            &buffer.buffer,
            &buffer.allocation,
            nullptr));

        if (buffer.flags >= BufferFlags::Addressable)
        {
            buffer.address = vkGetBufferDeviceAddress(device, Temp(VkBufferDeviceAddressInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = buffer.buffer,
            }));
        }
    }

    u64 VulkanContext::Buffer_GetSize(Buffer id)
    {
        return Get(id).size;
    }

    b8* VulkanContext::Buffer_GetMapped(Buffer buffer)
    {
        VmaAllocationInfo info;
        vmaGetAllocationInfo(vma, Get(buffer).allocation, &info);
        return reinterpret_cast<b8*>(info.pMappedData);
    }

    u64 VulkanContext::Buffer_GetAddress(Buffer id)
    {
        return Get(id).address;
    }

    void* VulkanContext::BufferImpl_Get(Buffer id, u64 index, u64 offset, usz stride)
    {
        return Buffer_GetMapped(id) + offset + (index * stride);
    }

    void VulkanContext::BufferImpl_Set(Buffer id, const void* data, usz count, u64 index, u64 offset, usz stride)
    {
        std::memcpy(BufferImpl_Get(id, index, offset, stride), data, count * stride);
    }

// -----------------------------------------------------------------------------

    void VulkanContext::Cmd_UpdateBuffer(CommandList cmd, Buffer dst, const void* pData, usz size, u64 dstOffset)
    {
        vkCmdUpdateBuffer(Get(cmd).buffer, Get(dst).buffer, dstOffset, size, pData);
    }

    void VulkanContext::Cmd_CopyToBuffer(CommandList cmd, Buffer dst, Buffer src, u64 size, u64 dstOffset, u64 srcOffset)
    {
        vkCmdCopyBuffer2(Get(cmd).buffer, Temp(VkCopyBufferInfo2 {
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .srcBuffer = Get(src).buffer,
            .dstBuffer = Get(dst).buffer,
            .regionCount = 1,
            .pRegions = Temp(VkBufferCopy2 {
                .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .srcOffset = srcOffset,
                .dstOffset = dstOffset,
                .size = size,
            }),
        }));
    }

    void VulkanContext::Cmd_Barrier(CommandList cmd, Buffer buffer, PipelineStage src, PipelineStage dst)
    {
        vkCmdPipelineBarrier2(Get(cmd).buffer, Temp(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = Temp(VkBufferMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask = GetVulkanPipelineStage(src),
                .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = GetVulkanPipelineStage(dst),
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .buffer = Get(buffer).buffer,
                .size = VK_WHOLE_SIZE,
            }),
        }));
    }
}