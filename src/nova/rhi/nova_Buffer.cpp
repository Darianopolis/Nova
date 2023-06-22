#include "nova_RHI_Impl.hpp"

#include <vk_mem_alloc.h>

namespace nova
{
    NOVA_DEFINE_HANDLE_OPERATIONS(Buffer)

    Buffer::Buffer(Context context, u64 size, BufferUsage usage, BufferFlags flags)
        : ImplHandle(new BufferImpl)
    {
        impl->context = context;
        impl->flags = flags;
        impl->usage = VkBufferUsageFlags(usage)
            | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        if (impl->flags >= BufferFlags::Addressable)
            impl->usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        impl->size = 0;
        Resize(size);
    }

    static
    void ResetBuffer(Buffer buffer)
    {
        if (!buffer->buffer)
            return;

        vmaDestroyBuffer(buffer->context->vma, buffer->buffer, buffer->allocation);
    }

    BufferImpl::~BufferImpl()
    {
        ResetBuffer(this);
    }

// -----------------------------------------------------------------------------

    void* Buffer::Get_(u64 index, u64 offset, usz stride) const noexcept
    {
        return GetMapped() + offset + (index * stride);
    }

    void Buffer::Set_(const void* data, usz count, u64 index, u64 offset, usz stride) const noexcept
    {
        std::memcpy(Get_(index, offset, stride), data, count * stride);
    }

// -----------------------------------------------------------------------------

    u64 Buffer::GetSize() const noexcept
    {
        return impl->size;
    }

    b8* Buffer::GetMapped() const noexcept
    {
        return impl->mapped;//static_cast<b8*>(impl->allocation->GetMappedData());
    }

    u64 Buffer::GetAddress() const noexcept
    {
        return impl->address;
    }

    void Buffer::Resize(u64 size) const
    {
        if (impl->size >= size)
            return;

        impl->size = size;

        ResetBuffer(*this);

        VmaAllocationCreateFlags vmaFlags = {};
        if (impl->flags >= BufferFlags::Mapped)
        {
            vmaFlags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
            if (impl->flags >= BufferFlags::DeviceLocal)
                vmaFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            else
                vmaFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        }

        VmaAllocationInfo info;

        VkCall(vmaCreateBuffer(
            impl->context->vma,
            Temp(VkBufferCreateInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = impl->size,
                .usage = impl->usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
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
            &info));

        if (impl->flags >= BufferFlags::Mapped)
            impl->mapped = static_cast<b8*>(info.pMappedData);

        if (impl->flags >= BufferFlags::Addressable)
        {
            impl->address = vkGetBufferDeviceAddress(impl->context->device, Temp(VkBufferDeviceAddressInfo {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = impl->buffer,
            }));
        }
    }

    void CommandList::UpdateBuffer(Buffer dst, const void* pData, usz size, u64 dstOffset) const
    {
        vkCmdUpdateBuffer(impl->buffer, dst->buffer, dstOffset, size, pData);
    }

    void CommandList::CopyToBuffer(Buffer dst, Buffer src, u64 size, u64 dstOffset, u64 srcOffset) const
    {
        vkCmdCopyBuffer2(impl->buffer, Temp(VkCopyBufferInfo2 {
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

    void CommandList::BindIndexBuffer(Buffer indexBuffer, IndexType indexType, u64 offset) const
    {
        vkCmdBindIndexBuffer(impl->buffer, indexBuffer->buffer, offset, VkIndexType(indexType));
    }
}