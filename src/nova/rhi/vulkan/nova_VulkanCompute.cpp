#include "nova_VulkanRHI.hpp"

namespace nova
{
    void CommandList::Dispatch(Vec3U groups) const
    {
        auto context = rhi::Get();

        context->vkCmdDispatch(impl->buffer, groups.x, groups.y, groups.z);
    }

    void CommandList::DispatchIndirect(HBuffer buffer, u64 offset) const
    {
        auto context = rhi::Get();

        context->vkCmdDispatchIndirect(impl->buffer, buffer->buffer, offset);
    }
}