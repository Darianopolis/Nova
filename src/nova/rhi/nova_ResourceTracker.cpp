#include "nova_RHI.hpp"

namespace nova
{
    ResourceTracker::ResourceTracker(Context _context)
        : context(_context.GetImpl())
    {}

    ResourceTracker::~ResourceTracker() {}

// -----------------------------------------------------------------------------

    void ResourceTracker::Clear(u32 maxAge)
    {
        clearList.clear();
        for (auto&[texture, state] : imageStates)
        {
            if (state.version + maxAge < version)
            {
                clearList.emplace_back(texture);
            }
            else
            if (state.version <= version)
            {
                state.layout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
        }

        for (auto texture : clearList)
            imageStates.erase(texture);

        version++;
    }

    void ResourceTracker::Reset(Texture texture)
    {
        Get(texture) = {};
    }

    void ResourceTracker::Persist(Texture texture)
    {
        Get(texture).version = version + 1;
    }

    void ResourceTracker::Set(Texture texture, VkImageLayout layout, VkPipelineStageFlags2 stage, VkAccessFlags2 access)
    {
        Get(texture) = {
            .layout = layout,
            .stage = stage,
            .access = access,
        };
    }

    ResourceTracker::ImageState& ResourceTracker::Get(Texture texture)
    {
        auto& state = imageStates[texture->image];
        state.version = std::max(state.version, version);
        return state;
    }
}