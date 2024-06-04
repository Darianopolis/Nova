#ifdef NOVA_PLATFORM_WINDOWS
#include <nova/rhi/vulkan/dxgi/nova_VulkanDXGISwapchain.hpp>
#include <nova/rhi/vulkan/gdi/nova_VulkanGDISwapchain.hpp>
#endif

#include <nova/rhi/vulkan/khr/nova_VulkanKHRSwapchain.hpp>

namespace nova
{
    // TODO: Sort by strategy instead of limiting to using the same strategy!

    SwapchainStrategy* PickStrategy(Span<HSwapchain> swapchains)
    {
        NOVA_ASSERT(!swapchains.empty(), "Must provide at least one swapchain");
        auto* strategy = swapchains[0]->strategy;
        for (u32 i = 1; i < swapchains.size(); ++i) {
            NOVA_ASSERT(swapchains[i]->strategy == strategy, "All swapchains must use the same strategy. Expected {} got {}", (void*)strategy, (void*)swapchains[i]->strategy);
        }
        return strategy;
    }

    SyncPoint Queue::Acquire(Span<HSwapchain> swapchains, bool* any_resized) const
    {
        return PickStrategy(swapchains)->Acquire(*this, swapchains, any_resized);
    }

    void Queue::Present(Span<HSwapchain> swapchains, Span<SyncPoint> waits, PresentFlag flags) const
    {
        PickStrategy(swapchains)->Present(*this, swapchains, waits, flags);
    }

    void Swapchain::Destroy()
    {
        if (!impl) return;
        impl->strategy->Destroy(*this);
        impl = nullptr;
    }

    Image Swapchain::Target() const
    {
        return impl->strategy->Target(*this);
    }

    Vec2U Swapchain::Extent() const
    {
        return impl->strategy->Extent(*this);
    }

    Format Swapchain::Format() const
    {
        return impl->strategy->Format(*this);
    }

    void CommandList::Present(HSwapchain swapchain) const
    {
        swapchain->strategy->PreparePresent(*this, swapchain);
    }

    Swapchain Swapchain::Create(Window window, ImageUsage usage, PresentMode present_mode, SwapchainFlags flags)
    {
        window->swapchain_handles_move_size = false;

        if (present_mode == PresentMode::Layered) {
#ifdef NOVA_PLATFORM_WINDOWS
            NOVA_ASSERT(flags >= SwapchainFlags::PreMultipliedAlpha, "Must specify PreMultipliedAlpha to use Layered currently");
            return GDISwapchain_Create(window, usage);
#else
            NOVA_THROW("Layered present mode not supported on this platform");
#endif
        }

        if (flags >= SwapchainFlags::PreMultipliedAlpha) {
            return DXGISwapchain_Create(window, usage, present_mode);
        } else {
            return KHRSwapchain_Create(window, usage, present_mode);
        }
    }
}