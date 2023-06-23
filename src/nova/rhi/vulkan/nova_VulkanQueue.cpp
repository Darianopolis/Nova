#include "nova_VulkanRHI.hpp"

namespace nova
{
    Queue VulkanContext::Queue_Get(QueueFlags flags)
    {
        // TODO: Filter based on flags
        (void)flags;

        return graphics;
    }

    void VulkanContext::Queue_Submit(Queue hQueue, Span<CommandList> _commandLists, Span<Fence> waits, Span<Fence> signals)
    {
        auto& queue = Get(hQueue);

        auto bufferInfos = NOVA_ALLOC_STACK(VkCommandBufferSubmitInfo, _commandLists.size());
        for (u32 i = 0; i < _commandLists.size(); ++i)
        {
            auto cmd = _commandLists[i];
            bufferInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = Get(cmd).buffer,
            };

            VkCall(vkEndCommandBuffer(Get(cmd).buffer));
        }

        auto waitInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, waits.size());
        for (u32 i = 0; i < waits.size(); ++i)
        {
            auto& wait = Get(waits[i]);
            waitInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = wait.semaphore,
                .value = wait.value,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
        }

        auto signalInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, signals.size());
        for (u32 i = 0; i < signals.size(); ++i)
        {
            auto& signal = Get(signals[i]);
            signalInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = signal.semaphore,
                .value = Fence_Advance(signals[i]),
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
        }

        auto start = std::chrono::steady_clock::now();
        VkCall(vkQueueSubmit2(queue.handle, 1, Temp(VkSubmitInfo2 {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = u32(waits.size()),
            .pWaitSemaphoreInfos = waitInfos,
            .commandBufferInfoCount = u32(_commandLists.size()),
            .pCommandBufferInfos = bufferInfos,
            .signalSemaphoreInfoCount = u32(signals.size()),
            .pSignalSemaphoreInfos = signalInfos,
        }), nullptr));
        TimeSubmitting += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
    }
}