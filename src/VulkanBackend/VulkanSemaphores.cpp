#include "VulkanBackend.hpp"

namespace pyr
{
    Ref<Semaphore> Context::MakeSemaphore()
    {
        Ref semaphore = new Semaphore;
        semaphore->context = this;

        VkCall(vkCreateSemaphore(device, Temp(VkSemaphoreCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = Temp(VkSemaphoreTypeCreateInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                .initialValue = 0,
            })
        }), nullptr, &semaphore->semaphore));

        return semaphore;
    }

    Semaphore::~Semaphore()
    {
        vkDestroySemaphore(context->device, semaphore, nullptr);
    }

    void Semaphore::Wait(u64 waitValue)
    {
        // PYR_LOG("Waiting on semaphore value - {}", value);
        VkCall(vkWaitSemaphores(context->device, Temp(VkSemaphoreWaitInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &semaphore,
            .pValues = waitValue ? &waitValue : &value,
        }), UINT64_MAX));
    }
}