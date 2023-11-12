#include "nova_VulkanRHI.hpp"

namespace nova
{
    void TransferManager::Init(HContext _context)
    {
        context = _context;

        queue = context.GetQueue(nova::QueueFlags::Transfer, 0);
        fence = nova::Fence::Create(context);
        cmd_pool = nova::CommandPool::Create(context, queue);
        staging = nova::Buffer::Create(context, 64ull * 1024 * 1024,
            nova::BufferUsage::TransferSrc,
            nova::BufferFlags::Mapped);
    }

    void TransferManager::Destroy()
    {
        staging.Destroy();
        cmd_pool.Destroy();
        fence.Destroy();
    }
}