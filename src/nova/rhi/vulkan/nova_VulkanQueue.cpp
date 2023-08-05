#include "nova_VulkanRHI.hpp"

namespace nova
{
    Queue::Queue(HContext _context)
        : Object(_context)
    {}

    Queue::~Queue()
    {}

    HQueue Context::GetQueue(QueueFlags flags, u32 index)
    {
        if (flags >= QueueFlags::Graphics)
        {
            if (index >= graphicQueues.size())
                NOVA_THROW("Tried to access graphics queue out of bounds - {}", index);
            return graphicQueues[index];
        }

        if (flags >= QueueFlags::Transfer)
            return transferQueues[index];

        if (flags >= QueueFlags::Compute)
            return computeQueues[index];

        NOVA_THROW("Illegal queue flags: {}", u32(flags));
    }

    void Queue::Submit(Span<HCommandList> _commandLists, Span<HFence> waits, Span<HFence> signals)
    {
        auto bufferInfos = NOVA_ALLOC_STACK(VkCommandBufferSubmitInfo, _commandLists.size());
        for (u32 i = 0; i < _commandLists.size(); ++i)
        {
            auto cmd = _commandLists[i];
            bufferInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = cmd->buffer,
            };

            VkCall(vkEndCommandBuffer(cmd->buffer));
        }

        auto waitInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, waits.size());
        for (u32 i = 0; i < waits.size(); ++i)
        {
            auto wait = waits[i];
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
            auto signal = signals[i];
            signalInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = signal->semaphore,
                .value = signal->Advance(),
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
        }

        auto start = std::chrono::steady_clock::now();
        VkCall(vkQueueSubmit2(handle, 1, Temp(VkSubmitInfo2 {
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