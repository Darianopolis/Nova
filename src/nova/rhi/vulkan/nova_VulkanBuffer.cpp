#include "nova_VulkanContext.hpp"

namespace nova
{
    Buffer VulkanContext::Buffer_Create(u64 size, BufferUsage usage, BufferFlags flags)
    {
        auto[id, buffer] = buffers.Acquire();

        buffer.flags = flags;
        buffer.usage = VkBufferUsageFlags(usage)
            | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        if (buffer.flags >= BufferFlags::Addressable)
            buffer.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        buffer.size = 0;

        Buffer_Resize(id, size);

        return id;
    }

    static
    void ResetBuffer(VulkanContext& ctx, VulkanBuffer& buffer)
    {
        if (buffer.buffer)
            return;

        vmaDestroyBuffer(ctx.vma, buffer.buffer, buffer.allocation);
    }

    void VulkanContext::Destroy(Buffer id)
    {
        ResetBuffer(*this, Get(id));
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

        VmaAllocationInfo info;

        VkCall(vmaCreateBuffer(
            vma,
            Temp(VkBufferCreateInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = buffer.size,
                .usage = buffer.usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
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
            &info));

        if (buffer.flags >= BufferFlags::Mapped)
            buffer.mapped = static_cast<b8*>(info.pMappedData);

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

    b8* VulkanContext::Buffer_GetMapped(Buffer id)
    {
        return Get(id).mapped;
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
}