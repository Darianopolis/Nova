#include "nova_RHI.hpp"

namespace nova
{
    Fence* Context::CreateFence()
    {
        auto _fence = new Fence;
        _fence->context = this;

        VkCall(vkCreateSemaphore(device, Temp(VkSemaphoreCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = Temp(VkSemaphoreTypeCreateInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                .initialValue = 0,
            })
        }), pAlloc, &_fence->semaphore));

        return _fence;
    }

    void Context::DestroyFence(Fence* _fence)
    {
        vkDestroySemaphore(device, _fence->semaphore, pAlloc);

        delete _fence;
    }

    void Fence::Wait(u64 waitValue)
    {
        VkCall(vkWaitSemaphores(context->device, Temp(VkSemaphoreWaitInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &semaphore,
            .pValues = waitValue ? &waitValue : &value,
        }), UINT64_MAX));
    }
}