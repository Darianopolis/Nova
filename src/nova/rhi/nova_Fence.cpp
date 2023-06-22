#include "nova_RHI_Impl.hpp"

namespace nova
{
    NOVA_DEFINE_HANDLE_OPERATIONS(Fence)

    Fence::Fence(Context context)
        : ImplHandle(new FenceImpl)
    {
        impl->context = context;
        VkCall(vkCreateSemaphore(context->device, Temp(VkSemaphoreCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = Temp(VkSemaphoreTypeCreateInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                .initialValue = 0,
            })
        }), context->pAlloc, &impl->semaphore));
    }

    FenceImpl::~FenceImpl()
    {
        if (semaphore)
            vkDestroySemaphore(context->device, semaphore, context->pAlloc);
    }

// -----------------------------------------------------------------------------

    void Fence::Wait(u64 waitValue) const
    {
        VkCall(vkWaitSemaphores(impl->context->device, Temp(VkSemaphoreWaitInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &impl->semaphore,
            .pValues = waitValue ? &waitValue : &impl->value,
        }), UINT64_MAX));
    }

    u64 Fence::GetPendingValue() const noexcept
    {
        return impl->value;
    }

    u64 Fence::Advance() const noexcept
    {
        return ++impl->value;
    }

    void Fence::Signal(u64 signalValue) const
    {
        VkCall(vkSignalSemaphore(impl->context->device, Temp(VkSemaphoreSignalInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
            .semaphore = impl->semaphore,
            .value = signalValue ? signalValue : impl->value,
        })));
    }
}