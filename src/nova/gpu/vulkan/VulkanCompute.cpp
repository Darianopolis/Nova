#include "VulkanRHI.hpp"

namespace nova
{
    void CommandList::Dispatch(Vec3U groups) const
    {
        impl->context->vkCmdDispatch(impl->buffer, groups.x, groups.y, groups.z);
    }

    void CommandList::DispatchIndirect(HBuffer buffer, u64 offset) const
    {
        impl->context->vkCmdDispatchIndirect(impl->buffer, buffer->buffer, offset);
    }
}