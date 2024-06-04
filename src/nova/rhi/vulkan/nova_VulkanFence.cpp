#include "nova_VulkanRHI.hpp"

namespace nova
{
    Fence Fence::Create()
    {
        auto context = rhi::Get();

        auto impl = new Impl;

        vkh::Check(context->vkCreateSemaphore(context->device, PtrTo(VkSemaphoreCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = PtrTo(VkSemaphoreTypeCreateInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                .initialValue = 0,
            }),
        }), context->alloc, &impl->semaphore));

        return { impl };
    }

    void Fence::Destroy()
    {
        auto context = rhi::Get();

        if (!impl) {
            return;
        }

        Wait();

        context->vkDestroySemaphore(context->device, impl->semaphore, context->alloc);

        delete impl;
        impl = nullptr;
    }

    void Fence::Wait(u64 wait_value) const
    {
        auto context = rhi::Get();

        if (wait_value == InvalidFenceValue) {
            wait_value = impl->last_submitted_value;
        }

        u64 last_seen = impl->last_seen_value;
        if (last_seen >= wait_value) {
            return;
        }

        vkh::Check(context->vkWaitSemaphores(context->device, PtrTo(VkSemaphoreWaitInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &impl->semaphore,
            .pValues = &wait_value,
        }), UINT64_MAX));

        AtomicSetMax(impl->last_seen_value, last_seen /* this saves one atomic load, worth? */, wait_value);
    }

    u64 Fence::Advance() const
    {
        return ++impl->last_submitted_value;
    }

    void Fence::AdvanceTo(u64 value) const
    {
        NOVA_ASSERT(value != InvalidFenceValue, "Must advance to a specific value");
        NOVA_ASSERT(value > impl->last_submitted_value, "Target value must be greater than last submitted value. Out of order submits must be resolved to a final value before calling AdvanceTo");

        impl->last_submitted_value = value;
    }

    void Fence::Signal(u64 signal_value) const
    {
        auto context = rhi::Get();

        if (signal_value == InvalidFenceValue) {
            signal_value = Advance();
        }

        if (signal_value < impl->last_submitted_value) {
            NOVA_ASSERT(signal_value < impl->last_submitted_value, "Signal value must be greater than last submitted value");
        }

        impl->last_submitted_value = signal_value;

        vkh::Check(context->vkSignalSemaphore(context->device, PtrTo(VkSemaphoreSignalInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
            .semaphore = impl->semaphore,
            .value = signal_value,
        })));
    }

    u64 Fence::PendingValue() const
    {
        return impl->last_submitted_value;
    }

    u64 Fence::CurrentValue() const
    {
        auto context = rhi::Get();

        u64 last_seen = impl->last_seen_value;

        if (last_seen >= impl->last_submitted_value) {
            // CurrentValue won't try and check for new values beyond submitted values set by Advance/AdvanceTo
            return last_seen;
        }

        u64 actual_value = 0;
        vkh::Check(context->vkGetSemaphoreCounterValue(context->device, impl->semaphore, &actual_value));

        AtomicSetMax(impl->last_seen_value, last_seen, actual_value);

        return actual_value;
    }

    bool Fence::Check(u64 check_value) const
    {
        auto context = rhi::Get();

        if (check_value == InvalidFenceValue) {
            check_value = impl->last_submitted_value;
        }

        u64 last_seen = impl->last_seen_value;

        if (last_seen >= check_value) {
            return true;
        }

        u64 actual_value = 0;
        vkh::Check(context->vkGetSemaphoreCounterValue(context->device, impl->semaphore, &actual_value));

        AtomicSetMax(impl->last_seen_value, last_seen, actual_value);

        return actual_value >= check_value;
    }
}