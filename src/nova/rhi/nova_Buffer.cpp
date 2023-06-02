#include "nova_RHI.hpp"

namespace nova
{
    Buffer::Buffer(Context& _context, u64 _size, BufferUsage _usage, BufferFlags _flags)
        : context(&_context)
        , flags(_flags)
    {
        usage = VkBufferUsageFlags(_usage) | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (flags >= BufferFlags::Addressable)
            usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        size = 0;
        Resize(_size);
    }

    static
    void ResetBuffer(Buffer& buffer)
    {
        if (!buffer.buffer)
            return;

        if (buffer.mapped && buffer.flags >= BufferFlags::Mappable)
            vmaUnmapMemory(buffer.context->vma, buffer.allocation);

        vmaDestroyBuffer(buffer.context->vma, buffer.buffer, buffer.allocation);
    }

    Buffer::~Buffer()
    {
        ResetBuffer(*this);
    }

    Buffer::Buffer(Buffer&& other) noexcept
        : context(other.context)
        , buffer(other.buffer)
        , allocation(other.allocation)
        , size(other.size)
        , address(other.address)
        , mapped(other.mapped)
        , flags(other.flags)
        , usage(other.usage)
    {
        other.buffer = nullptr;
        other.allocation = nullptr;
    }

// -----------------------------------------------------------------------------

    void Buffer::Resize(u64 _size)
    {
        if (size >= _size)
            return;

        size = _size;

        ResetBuffer(*this);

        VkCall(vmaCreateBuffer(
            context->vma,
            Temp(VkBufferCreateInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = size,
                .usage = usage,
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
            &buffer,
            &allocation,
            nullptr));

        if (flags >= BufferFlags::CreateMapped)
            VkCall(vmaMapMemory(context->vma, allocation, (void**)&mapped));

        if (flags >= BufferFlags::Addressable)
        {
            address = vkGetBufferDeviceAddress(context->device, Temp(VkBufferDeviceAddressInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = buffer,
            }));
        }
    }

    void CommandList::UpdateBuffer(Buffer& dst, const void* pData, usz size, u64 dstOffset)
    {
        vkCmdUpdateBuffer(buffer, dst.buffer, dstOffset, size, pData);
    }

    void CommandList::CopyToBuffer(Buffer& dst, Buffer& src, u64 size, u64 dstOffset, u64 srcOffset)
    {
        vkCmdCopyBuffer2(buffer, Temp(VkCopyBufferInfo2 {
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .srcBuffer = src.buffer,
            .dstBuffer = dst.buffer,
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