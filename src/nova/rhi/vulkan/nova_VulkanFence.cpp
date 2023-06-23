#include "nova_VulkanRHI.hpp"

namespace nova
{
    Fence VulkanContext::Fence_Create()
    {
        auto[id, fence] = fences.Acquire();

        VkCall(vkCreateSemaphore(device, Temp(VkSemaphoreCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = Temp(VkSemaphoreTypeCreateInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                .initialValue = 0,
            }),
        }), pAlloc, &fence.semaphore));

        return id;
    }

    void VulkanContext::Fence_Destroy(Fence id)
    {
        vkDestroySemaphore(device, Get(id).semaphore, pAlloc);
    }

    void VulkanContext::Fence_Wait(Fence id, u64 waitValue)
    {
        auto& fence = Get(id);

        VkCall(vkWaitSemaphores(device, Temp(VkSemaphoreWaitInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &fence.semaphore,
            .pValues = waitValue ? &waitValue : &fence.value,
        }), UINT64_MAX));
    }

    u64 VulkanContext::Fence_Advance(Fence id)
    {
        return ++Get(id).value;
    }

    void VulkanContext::Fence_Signal(Fence id, u64 signalValue)
    {
        auto& fence = Get(id);

        VkCall(vkSignalSemaphore(device, Temp(VkSemaphoreSignalInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
            .semaphore = fence.semaphore,
            .value = signalValue ? signalValue : fence.value,
        })));
    }

    u64 VulkanContext::Fence_GetPendingValue(Fence id)
    {
        return Get(id).value;
    }

}