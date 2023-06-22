#include "nova_VulkanContext.hpp"

namespace nova
{
    void VulkanContext::Queue_Submit(Span<CommandList> commandLists, Span<Fence> signals)
    {
        (void)commandLists;
        (void)signals;
    }

    void VulkanContext::Queue_Acquire(Span<Swapchain> _swapchains, Span<Fence> signals)
    {
        (void)_swapchains;
        (void)signals;
    }

    void VulkanContext::Queue_Present(Span<Swapchain> _swapchains, Span<Fence> waits, bool hostWait)
    {
        (void)_swapchains;
        (void)waits;
        (void)hostWait;
    }
}