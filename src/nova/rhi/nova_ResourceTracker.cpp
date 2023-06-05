#include "nova_RHI_Impl.hpp"

namespace nova
{
    NOVA_DEFINE_IMPL_HANDLE_OPERATIONS(ResourceTracker)

    ResourceTracker::ResourceTracker(Context context)
        : ImplHandle(new ResourceTrackerImpl)
    {
        impl->context = context.GetImpl();
    }

// -----------------------------------------------------------------------------

    void ResourceTracker::Clear(u32 maxAge) const noexcept
    {
        impl->clearList.clear();
        for (auto&[texture, state] : impl->imageStates)
        {
            if (state.version + maxAge < impl->version)
            {
                impl->clearList.emplace_back(texture);
            }
            else
            if (state.version <= impl->version)
            {
                state.layout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
        }

        for (auto texture : impl->clearList)
            impl->imageStates.erase(texture);

        impl->version++;
    }

    void ResourceTracker::Reset(Texture texture) const noexcept
    {
        impl->Get(texture) = {};
    }

    void ResourceTracker::Persist(Texture texture) const noexcept
    {
        impl->Get(texture).version = impl->version + 1;
    }

    void ResourceTracker::Set(Texture texture, VkImageLayout layout, VkPipelineStageFlags2 stage, VkAccessFlags2 access) const noexcept
    {
        impl->Get(texture) = {
            .layout = layout,
            .stage = stage,
            .access = access,
        };
    }

    ResourceTrackerImpl::ImageState& ResourceTrackerImpl::Get(Texture texture) noexcept
    {
        auto& state = imageStates[texture->image];
        state.version = std::max(state.version, version);
        return state;
    }
}