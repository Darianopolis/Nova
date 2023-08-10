#include "nova_VulkanRHI.hpp"

namespace nova
{
    Fence Fence::Create(Context context)
    {
        auto impl = new Impl;
        impl->context = context;

        VkCall(vkCreateSemaphore(context->device, Temp(VkSemaphoreCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = Temp(VkSemaphoreTypeCreateInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                .initialValue = 0,
            }),
        }), context->pAlloc, &impl->semaphore));

        return { impl };
    }

    void Fence::Destroy()
    {
        vkDestroySemaphore(impl->context->device, impl->semaphore, impl->context->pAlloc);

        delete impl;
        impl = nullptr;
    }

    void Fence::Wait(u64 waitValue) const
    {
        VkCall(vkWaitSemaphores(impl->context->device, Temp(VkSemaphoreWaitInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &impl->semaphore,
            .pValues = (waitValue != ~0ull) ? &waitValue : &impl->value,
        }), UINT64_MAX));
    }

    u64 Fence::Advance() const
    {
        return ++impl->value;
    }

    void Fence::Signal(u64 signalValue) const
    {
        VkCall(vkSignalSemaphore(impl->context->device, Temp(VkSemaphoreSignalInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
            .semaphore = impl->semaphore,
            .value = (signalValue != ~0ull) ? signalValue : impl->value,
        })));
    }

    u64 Fence::GetPendingValue() const
    {
        return impl->value;
    }

}