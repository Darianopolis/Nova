#include "nova_RHI.hpp"

namespace nova
{
    Buffer* Context::CreateBuffer(u64 size, BufferUsage _usage, BufferFlags flags)
    {
        auto buffer = new Buffer;
        NOVA_ON_SCOPE_FAILURE(&) { DestroyBuffer(buffer); };
        buffer->context = this;
        buffer->flags = flags;

        buffer->usage = VkBufferUsageFlags(_usage) | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (flags >= BufferFlags::Addressable)
            buffer->usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VkCall(vmaCreateBuffer(
            vma,
            Temp(VkBufferCreateInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = size,
                .usage = buffer->usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            }),
            Temp(VmaAllocationCreateInfo {
                .flags = (flags >= BufferFlags::Mappable)
                    ? ((flags >= BufferFlags::DeviceLocal)
                        ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                        : VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)
                    : VmaAllocationCreateFlags(0),
                .usage = VMA_MEMORY_USAGE_AUTO,
                .requiredFlags = (flags >= BufferFlags::DeviceLocal)
                    ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                    : VkMemoryPropertyFlags(0),
            }),
            &buffer->buffer,
            &buffer->allocation,
            nullptr));

        buffer->size = size;

        if (flags >= BufferFlags::CreateMapped)
            VkCall(vmaMapMemory(vma, buffer->allocation, (void**)&buffer->mapped));

        if (flags >= BufferFlags::Addressable)
        {
            buffer->address = vkGetBufferDeviceAddress(device, Temp(VkBufferDeviceAddressInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = buffer->buffer,
            }));
        }

        return buffer;
    }

    void Context::DestroyBuffer(Buffer* buffer)
    {
        if (!buffer)
            return;

        if (buffer->mapped && buffer->flags >= BufferFlags::Mappable)
            vmaUnmapMemory(vma, buffer->allocation);

        vmaDestroyBuffer(vma, buffer->buffer, buffer->allocation);

        delete buffer;
    }


    void CommandList::UpdateBuffer(Buffer* dst, const void* pData, usz size, u64 dstOffset)
    {
        vkCmdUpdateBuffer(buffer, dst->buffer, dstOffset, size, pData);
    }

    void CommandList::CopyToBuffer(Buffer* dst, Buffer* src, u64 size, u64 dstOffset, u64 srcOffset)
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
}