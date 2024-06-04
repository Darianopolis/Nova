#include "nova_VulkanRHI.hpp"

namespace nova
{
    void TransferManager::Init()
    {
        auto context = rhi::Get();

        // TODO: Also handle transfers for buffers
        // TODO: Configure transfer size, lazily create?
        if (!staged_image_copy) {
            return;
        }

        queue = Queue::Get(nova::QueueFlags::Transfer, 0);
        staging = nova::Buffer::Create(64ull * 1024 * 1024,
            nova::BufferUsage::TransferSrc,
            nova::BufferFlags::Mapped);
    }

    void TransferManager::Destroy()
    {
        staging.Destroy();
    }
}