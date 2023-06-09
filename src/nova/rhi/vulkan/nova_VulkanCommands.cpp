#include "nova_VulkanRHI.hpp"

namespace nova
{
    CommandPool VulkanContext::Commands_CreatePool(Queue queue)
    {
        auto[id, pool] = commandPools.Acquire();

        pool.queue = queue;

        VkCall(vkCreateCommandPool(device, Temp(VkCommandPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = Get(queue).family,
        }), pAlloc, &pool.pool));

        return id;
    }

    void VulkanContext::Commands_DestroyPool(CommandPool id)
    {
        auto& pool = Get(id);
        if (pool.pool)
            vkDestroyCommandPool(device, pool.pool, pAlloc);

        for (auto list : pool.lists)
            commandLists.Return(list);

        commandPools.Return(id);
    }

    CommandList VulkanContext::Commands_Begin(CommandPool hPool, CommandState state)
    {
        auto& pool = Get(hPool);

        CommandList cmd;
        if (pool.index >= pool.lists.size())
        {
            auto[hList, list] = commandLists.Acquire();
            cmd = hList;
            pool.lists.emplace_back(cmd);

            list.pool = hPool;
            VkCall(vkAllocateCommandBuffers(device, Temp(VkCommandBufferAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = pool.pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            }), &list.buffer));
            pool.index++;
        }
        else
        {
            cmd = pool.lists[pool.index++];
        }

        VkCall(vkBeginCommandBuffer(Get(cmd).buffer, Temp(VkCommandBufferBeginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        })));

        Get(cmd).state = state;

        return cmd;
    }

    void VulkanContext::Commands_Reset(CommandPool id)
    {
        auto& pool = Get(id);

        pool.index = 0;
        VkCall(vkResetCommandPool(device, pool.pool, 0));
    }

// -----------------------------------------------------------------------------

    CommandState VulkanContext::Commands_CreateState()
    {
        auto[id, state] = commandStates.Acquire();
        return id;
    }

    void VulkanContext::Commands_DestroyState(CommandState id)
    {
        commandStates.Return(id);
    }

    void VulkanContext::Commands_SetState(CommandState state, Texture texture,
        VkImageLayout layout, VkPipelineStageFlags2 stages, VkAccessFlags2 access)
    {
        (void)state;
        (void)texture;
        (void)layout;
        (void)stages;
        (void)access;
    }

    void VulkanContext::Cmd_Barrier(CommandList cmd, PipelineStage src, PipelineStage dst)
    {
        vkCmdPipelineBarrier2(Get(cmd).buffer, Temp(VkDependencyInfo {
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