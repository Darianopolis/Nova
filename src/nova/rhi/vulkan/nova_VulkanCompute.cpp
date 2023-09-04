#include "nova_VulkanRHI.hpp"

namespace nova
{
    void CommandList::Dispatch(Vec3U groups) const
    {
        vkCmdDispatch(impl->buffer, groups.x, groups.y, groups.z);
    }
}