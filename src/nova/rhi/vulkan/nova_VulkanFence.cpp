#include "nova_VulkanRHI.hpp"

namespace nova
{
    Fence Fence::Create(HContext context)
    {
        auto impl = new Impl;
        impl->context = context;

        vkh::Check(impl->context->vkCreateSemaphore(context->device, Temp(VkSemaphoreCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = Temp(VkSemaphoreTypeCreateInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                .initialValue = 0,
            }),
        }), context->alloc, &impl->semaphore));

        return { impl };
    }

    void Fence::Destroy()
    {
        if (!impl) {
            return;
        }

        impl->context->vkDestroySemaphore(impl->context->device, impl->semaphore, impl->context->alloc);

        delete impl;
        impl = nullptr;
    }

    void Fence::Wait(u64 wait_value) const
    {
        vkh::Check(impl->context->vkWaitSemaphores(impl->context->device, Temp(VkSemaphoreWaitInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &impl->semaphore,
            .pValues = (wait_value != ~0ull) ? &wait_value : &impl->value,
        }), UINT64_MAX));
    }

    u64 Fence::Advance() const
    {
        return ++impl->value;
    }

    void Fence::Signal(u64 signal_value) const
    {
        vkh::Check(impl->context->vkSignalSemaphore(impl->context->device, Temp(VkSemaphoreSignalInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
            .semaphore = impl->semaphore,
            .value = (signal_value != ~0ull) ? signal_value : impl->value,
        })));
    }

    u64 Fence::PendingValue() const
    {
        return impl->value;
    }

    u64 Fence::CurrentValue() const
    {
        u64 actual_value = 0;
        vkh::Check(impl->context->vkGetSemaphoreCounterValue(impl->context->device, impl->semaphore, &actual_value));
        return actual_value;
    }

    bool Fence::Check(u64 check_value) const
    {
        return CurrentValue() >= ((check_value != ~0ull) ? check_value : impl->value);
    }
}