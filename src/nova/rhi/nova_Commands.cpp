#include "nova_RHI.hpp"

namespace nova
{
    Commands* Context::CreateCommands()
    {
        auto cmds = new Commands;
        cmds->context = this;
        cmds->queue = graphics;

        VkCall(vkCreateCommandPool(device, Temp(VkCommandPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = cmds->queue->family,
        }), pAlloc, &cmds->pool));

        return cmds;
    }

    void Context::DestroyCommands(Commands* commands)
    {
        vkDestroyCommandPool(device, commands->pool, pAlloc);

        delete commands;
    }

    VkCommandBuffer Commands::Allocate()
    {
        VkCommandBuffer cmd;
        if (index >= buffers.size())
        {
            VkCommandBuffer& newCmd = buffers.emplace_back();
            VkCall(vkAllocateCommandBuffers(context->device, Temp(VkCommandBufferAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            }), &newCmd));

            cmd = newCmd;
            index++;
        }
        else
        {
            cmd = buffers[index++];
        }

        VkCall(vkBeginCommandBuffer(cmd, Temp(VkCommandBufferBeginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        })));

        return cmd;
    }

    void Commands::Clear()
    {
        // if (queue && index)
        // {
        //     auto bufferInfos = (VkCommandBufferSubmitInfo*)_alloca(index * sizeof(VkCommandBufferSubmitInfo));
        //     for (u32 i = 0; i < index; ++i)
        //     {
        //         bufferInfos[i] = {
        //             .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        //             .commandBuffer = buffers[i],
        //         };

        //         VkCall(vkEndCommandBuffer(buffers[i]));
        //     }

        //     VkCall(vkQueueSubmit2(queue->handle, 1, Temp(VkSubmitInfo2 {
        //         .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        //         .commandBufferInfoCount = index,
        //         .pCommandBufferInfos = bufferInfos,
        //     }), VK_NULL_HANDLE));

        //     _freea(bufferInfos);

        //     VkCall(vkQueueWaitIdle(queue->handle));
        // }

        index = 0;
        VkCall(vkResetCommandPool(context->device, pool, 0));
    }

    void Queue::Submit(VkCommandBuffer cmd, Fence* wait, Fence* signal)
    {
        VkCall(vkEndCommandBuffer(cmd));

        VkCall(vkQueueSubmit2(handle, 1, Temp(VkSubmitInfo2 {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = wait ? 1u : 0u,
            .pWaitSemaphoreInfos = wait ? Temp(VkSemaphoreSubmitInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = wait->semaphore,
                .value = wait->value,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            }) : nullptr,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = Temp(VkCommandBufferSubmitInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = cmd,
            }),
            .signalSemaphoreInfoCount = signal ? 1u : 0u,
            .pSignalSemaphoreInfos = signal ? Temp(VkSemaphoreSubmitInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = signal->semaphore,
                .value = ++signal->value,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            }): nullptr,
        }), nullptr));
    }
}