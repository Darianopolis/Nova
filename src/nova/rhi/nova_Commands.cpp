#include "nova_RHI.hpp"

namespace nova
{
    CommandPool* Context::CreateCommands()
    {
        auto cmds = new CommandPool;
        cmds->context = this;
        cmds->queue = graphics;

        VkCall(vkCreateCommandPool(device, Temp(VkCommandPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = cmds->queue->family,
        }), pAlloc, &cmds->pool));

        return cmds;
    }

    void Context::Destroy(CommandPool* pool)
    {
        vkDestroyCommandPool(device, pool->pool, pAlloc);

        for (auto& cmd : pool->buffers)
            delete cmd;

        delete pool;
    }

    CommandList* CommandPool::Begin()
    {
        CommandList* cmd;
        if (index >= buffers.size())
        {
            cmd = buffers.emplace_back(new CommandList);
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
            cmd = buffers[index++];
        }

        VkCall(vkBeginCommandBuffer(cmd->buffer, Temp(VkCommandBufferBeginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        })));

        return cmd;
    }

    void CommandPool::Reset()
    {
        index = 0;
        VkCall(vkResetCommandPool(context->device, pool, 0));
    }

    NOVA_NO_INLINE
    void Queue::Submit(std::initializer_list<CommandList*> commandLists, std::initializer_list<Fence*> waits, std::initializer_list<Fence*> signals)
    {
        auto bufferInfos = NOVA_ALLOC_STACK(VkCommandBufferSubmitInfo, commandLists.size());
        for (u32 i = 0; i < commandLists.size(); ++i)
        {
            auto cmd = commandLists.begin()[i];
            bufferInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = cmd->buffer,
            };

            VkCall(vkEndCommandBuffer(cmd->buffer));
        }

        auto waitInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, waits.size());
        for (u32 i = 0; i < waits.size(); ++i)
        {
            auto wait = waits.begin()[i];
            waitInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = wait->semaphore,
                .value = wait->value,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
        }

        auto signalInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, signals.size());
        for (u32 i = 0; i < signals.size(); ++i)
        {
            auto signal = signals.begin()[i];
            signalInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = signal->semaphore,
                .value = ++signal->value,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
        }

        VkCall(vkQueueSubmit2(handle, 1, Temp(VkSubmitInfo2 {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = u32(waits.size()),
            .pWaitSemaphoreInfos = waitInfos,
            .commandBufferInfoCount = u32(commandLists.size()),
            .pCommandBufferInfos = bufferInfos,
            .signalSemaphoreInfoCount = u32(signals.size()),
            .pSignalSemaphoreInfos = signalInfos,
        }), nullptr));
    }
}