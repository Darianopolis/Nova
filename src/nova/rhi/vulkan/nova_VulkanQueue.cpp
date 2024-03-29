#include "nova_VulkanRHI.hpp"

#include <nova/core/nova_Stack.hpp>

namespace nova
{
    Queue Context::Queue(QueueFlags flags, u32 index) const
    {
        if (flags >= QueueFlags::Graphics) {
            if (index >= impl->graphics_queues.size()) {
                NOVA_THROW("Tried to access graphics queue out of bounds - {}", index);
            }
            return impl->graphics_queues[index];
        }

        if (flags >= QueueFlags::Transfer) {
            return impl->transfer_queues[index];
        }

        if (flags >= QueueFlags::Compute) {
            return impl->compute_queues[index];
        }

        NOVA_THROW("Illegal queue flags: {}", u32(flags));
    }

    FenceValue Queue::Submit(Span<HCommandList> command_lists, Span<FenceValue> waits) const
    {
        NOVA_STACK_POINT();

        auto buffer_infos = NOVA_STACK_ALLOC(VkCommandBufferSubmitInfo, command_lists.size());
        for (u32 i = 0; i < command_lists.size(); ++i) {
            auto cmd = command_lists[i];
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
                .semaphore = wait.fence->semaphore,
                .value = wait.Value(),
                // TODO: Additional granularity?
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
        }

        VkSemaphoreSubmitInfo signal_info {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = impl->fence->semaphore,
            .value = impl->fence.Advance(),
            // TODO: Additional granularity?
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        };

        auto start = std::chrono::steady_clock::now();
        vkh::Check(impl->context->vkQueueSubmit2(impl->handle, 1, Temp(VkSubmitInfo2 {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = u32(waits.size()),
            .pWaitSemaphoreInfos = wait_infos,
            .commandBufferInfoCount = u32(command_lists.size()),
            .pCommandBufferInfos = buffer_infos,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signal_info,
        }), nullptr));

        rhi::stats::TimeSubmitting += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();

        return impl->fence;
    }

    Fence Queue::Internal_Fence() const
    {
        return impl->fence;
    }

    void Queue::WaitIdle() const
    {
        impl->fence.Wait();
    }
}