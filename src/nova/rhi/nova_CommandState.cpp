#include "nova_RHI_Impl.hpp"

namespace nova
{
    NOVA_DEFINE_HANDLE_OPERATIONS(CommandState)

    CommandState::CommandState(Context context)
        : ImplHandle(new CommandStateImpl)
    {
        impl->context = context.GetImpl();
    }

// -----------------------------------------------------------------------------

    void CommandState::Clear(u32 maxAge) const noexcept
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

    void CommandState::Reset(Texture texture) const noexcept
    {
        impl->Get(texture) = {};
    }

    void CommandState::Persist(Texture texture) const noexcept
    {
        impl->Get(texture).version = impl->version + 1;
    }

    void CommandState::Set(Texture texture, VkImageLayout layout, VkPipelineStageFlags2 stage, VkAccessFlags2 access) const noexcept
    {
        impl->Get(texture) = {
            .layout = layout,
            .stage = stage,
            .access = access,
        };
    }

    CommandStateImpl::ImageState& CommandStateImpl::Get(Texture texture) noexcept
    {
        auto& state = imageStates[texture->image];
        state.version = std::max(state.version, version);
        return state;
    }
}