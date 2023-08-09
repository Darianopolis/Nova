#include "nova_VulkanRHI.hpp"

namespace nova
{
    HCommandPool CommandPool::Create(HContext context, HQueue queue)
    {
        auto impl = new CommandPool;
        impl->context = context;
        impl->queue = queue;

        VkCall(vkCreateCommandPool(context->device, Temp(VkCommandPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = queue->family,
        }), context->pAlloc, &impl->pool));

        return impl;
    }

    CommandPool::~CommandPool()
    {
        for (auto& list : lists)
            list.Destroy();

        vkDestroyCommandPool(context->device, pool, context->pAlloc);
    }

    HCommandList CommandPool::Begin(HCommandState state)
    {
        HCommandList cmd;
        if (index >= lists.size())
        {
            cmd = lists.emplace_back(new CommandList);

            cmd->pool = this;
            VkCall(vkAllocateCommandBuffers(context->device, Temp(VkCommandBufferAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            }), &cmd->buffer));
            index++;
        }
        else
        {
            cmd = lists[index++];
        }

        VkCall(vkBeginCommandBuffer(cmd->buffer, Temp(VkCommandBufferBeginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        })));

        cmd->state = state;

        return cmd;
    }

    void CommandPool::Reset()
    {
        index = 0;
        VkCall(vkResetCommandPool(context->device, pool, 0));
    }

// -----------------------------------------------------------------------------

    HCommandState CommandState::Create(HContext context)
    {
        auto impl = new CommandState;
        impl->context = context;

        return impl;
    }

    CommandState::~CommandState()
    {}

    void CommandState::SetState(HTexture texture,
        VkImageLayout layout, VkPipelineStageFlags2 stages, VkAccessFlags2 access)
    {
        // TODO
        (void)texture;
        (void)layout;
        (void)stages;
        (void)access;
    }

    void CommandList::Barrier(PipelineStage src, PipelineStage dst)
    {
        vkCmdPipelineBarrier2(buffer, Temp(VkDependencyInfo {
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