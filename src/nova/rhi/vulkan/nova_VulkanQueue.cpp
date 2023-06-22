#include "nova_VulkanContext.hpp"

namespace nova
{
    Queue VulkanContext::Queue_Get(QueueFlags flags)
    {
        // TODO: Filter based on flags
        (void)flags;

        return graphics;
    }

    void VulkanContext::Queue_Submit(Queue, Span<CommandList> commandLists, Span<Fence> signals)
    {
        (void)commandLists;
        (void)signals;
    }
}