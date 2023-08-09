#include "nova_VulkanRHI.hpp"

namespace nova
{
    HFence Fence::Create(HContext context)
    {
        auto impl = new Fence;
        impl->context = context;

        VkCall(vkCreateSemaphore(context->device, Temp(VkSemaphoreCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = Temp(VkSemaphoreTypeCreateInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                .initialValue = 0,
            }),
        }), context->pAlloc, &impl->semaphore));

        return impl;
    }

    Fence::~Fence()
    {
        vkDestroySemaphore(context->device, semaphore, context->pAlloc);
    }

    void Fence::Wait(u64 waitValue)
    {
        VkCall(vkWaitSemaphores(context->device, Temp(VkSemaphoreWaitInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &semaphore,
            .pValues = (waitValue != ~0ull) ? &waitValue : &value,
        }), UINT64_MAX));
    }

    u64 Fence::Advance()
    {
        return ++value;
    }

    void Fence::Signal(u64 signalValue)
    {
        VkCall(vkSignalSemaphore(context->device, Temp(VkSemaphoreSignalInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
            .semaphore = semaphore,
            .value = (signalValue != ~0ull) ? signalValue : value,
        })));
    }

    u64 Fence::GetPendingValue()
    {
        return value;
    }

}