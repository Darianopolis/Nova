#include "nova_VulkanRHI.hpp"

namespace nova
{
    Queue Context::GetQueue(QueueFlags flags, u32 index) const
    {
        if (flags >= QueueFlags::Graphics) {
            if (index >= impl->graphicQueues.size()) {
                NOVA_THROW("Tried to access graphics queue out of bounds - {}", index);
            }
            return impl->graphicQueues[index];
        }

        if (flags >= QueueFlags::Transfer) {
            return impl->transferQueues[index];
        }

        if (flags >= QueueFlags::Compute) {
            return impl->computeQueues[index];
        }

        NOVA_THROW("Illegal queue flags: {}", u32(flags));
    }

    void Queue::Submit(Span<HCommandList> _commandLists, Span<HFence> waits, Span<HFence> signals) const
    {
        auto bufferInfos = NOVA_ALLOC_STACK(VkCommandBufferSubmitInfo, _commandLists.size());
        for (u32 i = 0; i < _commandLists.size(); ++i) {
            auto cmd = _commandLists[i];
            bufferInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = cmd->buffer,
            };

            vkh::Check(vkEndCommandBuffer(cmd->buffer));
        }

        auto waitInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, waits.size());
        for (u32 i = 0; i < waits.size(); ++i) {
            auto wait = waits[i];
            waitInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = wait->semaphore,
                .value = wait->value,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
        }

        auto signalInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, signals.size());
        for (u32 i = 0; i < signals.size(); ++i) {
            auto signal = signals[i];
            signalInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = signal->semaphore,
                .value = signal.Unwrap().Advance(),
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
        }

        auto start = std::chrono::steady_clock::now();
        vkh::Check(vkQueueSubmit2(impl->handle, 1, Temp(VkSubmitInfo2 {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = u32(waits.size()),
            .pWaitSemaphoreInfos = waitInfos,
            .commandBufferInfoCount = u32(_commandLists.size()),
            .pCommandBufferInfos = bufferInfos,
            .signalSemaphoreInfoCount = u32(signals.size()),
            .pSignalSemaphoreInfos = signalInfos,
        }), nullptr));
        rhi::stats::TimeSubmitting += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
    }
}