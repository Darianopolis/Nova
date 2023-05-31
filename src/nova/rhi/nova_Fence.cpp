#include "nova_RHI.hpp"

namespace nova
{
    Rc<Fence> Context::CreateFence()
    {
        Rc fence = new Fence;
        fence->context = this;

        VkCall(vkCreateSemaphore(device, Temp(VkSemaphoreCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = Temp(VkSemaphoreTypeCreateInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                .initialValue = 0,
            })
        }), pAlloc, &fence->semaphore));

        return fence;
    }

    Fence::~Fence()
    {
        NOVA_LOGEXPR(context);
        NOVA_LOGEXPR(semaphore);
        vkDestroySemaphore(context->device, semaphore, context->pAlloc);
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


    u64 Fence::Advance()
    {
        return ++value;
    }

    void Fence::Signal(u64 _value)
    {
        auto res = vkSignalSemaphore(context->device, Temp(VkSemaphoreSignalInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
            .semaphore = semaphore,
            .value = _value,
        }));
        VkCall(res);
    }
}