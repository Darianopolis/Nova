#include "VulkanBackend.hpp"

namespace pyr
{
    Ref<Buffer> Context::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, BufferFlags flags)
    {
        Ref buffer = new Buffer;
        buffer->context = this;
        buffer->flags = flags;

        usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (flags >= BufferFlags::Addressable)
            usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VkCall(vmaCreateBuffer(
            vma,
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

    Buffer::~Buffer()
    {
        if (mapped && flags >= BufferFlags::Mappable)
            vmaUnmapMemory(context->vma, allocation);
    }

    void Context::CopyToBuffer(Buffer& buffer, const void* data, size_t size, VkDeviceSize offset)
    {
        if (!data)
        {
            vkCmdCopyBuffer(transferCmd, staging->buffer, buffer.buffer, 1, Temp(VkBufferCopy {
                .srcOffset = 0,
                .dstOffset = offset,
                .size = size,
            }));

            Flush(transferCmd);
        }
        else if (buffer.mapped)
        {
            std::memcpy(buffer.mapped + offset, data, size);
        }
        else
        {
            for (u64 start = 0; start < size; start += staging->size)
            {
                u64 chunkSize = std::min(size, start + staging->size) - start;

                std::memcpy(staging->mapped, (byte*)data + start, chunkSize);

                vkCmdCopyBuffer(transferCmd, staging->buffer, buffer.buffer, 1, Temp(VkBufferCopy {
                    .srcOffset = 0,
                    .dstOffset = start + offset,
                    .size = chunkSize,
                }));

                Flush(transferCmd);
            }
        }
    }
}