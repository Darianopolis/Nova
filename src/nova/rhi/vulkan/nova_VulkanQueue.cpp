#include "nova_VulkanRHI.hpp"

#include <nova/core/nova_Stack.hpp>

namespace nova
{
    Queue Context::GetQueue(QueueFlags flags, u32 index) const
    {
        if (flags >= QueueFlags::Graphics) {
            if (index >= impl->graphic_queues.size()) {
                NOVA_THROW("Tried to access graphics queue out of bounds - {}", index);
            }
            return impl->graphic_queues[index];
        }

        if (flags >= QueueFlags::Transfer) {
            return impl->transfer_queues[index];
        }

        if (flags >= QueueFlags::Compute) {
            return impl->compute_queues[index];
        }

        NOVA_THROW("Illegal queue flags: {}", u32(flags));
    }

    void Queue::Submit(Span<HCommandList> _command_lists, Span<HFence> waits, Span<HFence> signals) const
    {
        NOVA_STACK_POINT();

        auto buffer_infos = NOVA_STACK_ALLOC(VkCommandBufferSubmitInfo, _command_lists.size());
        for (u32 i = 0; i < _command_lists.size(); ++i) {
            auto cmd = _command_lists[i];
            buffer_infos[i] = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = cmd->buffer,
            };

            vkh::Check(impl->context->vkEndCommandBuffer(cmd->buffer));
        }

        auto wait_infos = NOVA_STACK_ALLOC(VkSemaphoreSubmitInfo, waits.size());
        for (u32 i = 0; i < waits.size(); ++i) {
            auto wait = waits[i];
            wait_infos[i] = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = wait->semaphore,
                .value = wait->value,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
        }

        auto signal_infos = NOVA_STACK_ALLOC(VkSemaphoreSubmitInfo, signals.size());
        for (u32 i = 0; i < signals.size(); ++i) {
            auto signal = signals[i];
            signal_infos[i] = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = signal->semaphore,
                .value = signal.Unwrap().Advance(),
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
        }

        auto start = std::chrono::steady_clock::now();
        vkh::Check(impl->context->vkQueueSubmit2(impl->handle, 1, Temp(VkSubmitInfo2 {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = u32(waits.size()),
            .pWaitSemaphoreInfos = wait_infos,
            .commandBufferInfoCount = u32(_command_lists.size()),
            .pCommandBufferInfos = buffer_infos,
            .signalSemaphoreInfoCount = u32(signals.size()),
            .pSignalSemaphoreInfos = signal_infos,
        }), nullptr));

        rhi::stats::TimeSubmitting += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
    }
}