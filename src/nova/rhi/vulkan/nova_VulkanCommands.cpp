#include "nova_VulkanRHI.hpp"

namespace nova
{
    CommandPool CommandPool::Create(HContext context, HQueue queue)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->queue = queue;

        VkCall(vkCreateCommandPool(context->device, Temp(VkCommandPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = queue->family,
        }), context->pAlloc, &impl->pool));

        return { impl };
    }

    void CommandPool::Destroy()
    {
        if (!impl) return;
        
        for (auto& list : impl->lists)
            delete list.impl;

        vkDestroyCommandPool(impl->context->device, impl->pool, impl->context->pAlloc);

        delete impl;
        impl = nullptr;
    }

    CommandList CommandPool::Begin(HCommandState state) const
    {
        CommandList cmd;
        if (impl->index >= impl->lists.size())
        {
            cmd = impl->lists.emplace_back(new CommandList::Impl);

            cmd->pool = *this;
            VkCall(vkAllocateCommandBuffers(impl->context->device, Temp(VkCommandBufferAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = impl->pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            }), &cmd->buffer));
            impl->index++;
        }
        else
        {
            cmd = impl->lists[impl->index++];
        }

        VkCall(vkBeginCommandBuffer(cmd->buffer, Temp(VkCommandBufferBeginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        })));

        cmd->state = state;

        return cmd;
    }

    void CommandPool::Reset() const
    {
        impl->index = 0;
        VkCall(vkResetCommandPool(impl->context->device, impl->pool, 0));
    }

// -----------------------------------------------------------------------------

    CommandState CommandState::Create(HContext context)
    {
        auto impl = new Impl;
        impl->context = context;

        return { impl };
    }

    void CommandState::Destroy()
    {
        delete impl;
        impl = nullptr;
    }

    void CommandState::SetState(HTexture texture,
        VkImageLayout layout, VkPipelineStageFlags2 stages, VkAccessFlags2 access) const
    {
        // TODO
        (void)texture;
        (void)layout;
        (void)stages;
        (void)access;
    }

    void CommandList::Barrier(PipelineStage src, PipelineStage dst) const
    {
        vkCmdPipelineBarrier2(impl->buffer, Temp(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = Temp(VkMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = GetVulkanPipelineStage(src),
                .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = GetVulkanPipelineStage(dst),
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
            }),
        }));
    }
}