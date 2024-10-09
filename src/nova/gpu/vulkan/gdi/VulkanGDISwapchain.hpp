#include <nova/gpu/vulkan/VulkanRHI.hpp>
#include <nova/window/win32/Win32Window.hpp>

#include <nova/core/win32/Win32.hpp>

namespace nova
{
    struct GDISwapchainData : Handle<Swapchain>::Impl
    {
        Context context = {};

        Window        window = {};
        ImageUsage     usage = {};
        Image          image = {};
        HDC       hdc_screen = {};
        HANDLE          file = {};
        bool file_map_bitmap = false;
        HBITMAP       bitmap = {};
        void*           bits = {};
        Buffer        buffer;

        Vec2U bitmap_size = {};
        Vec2I    last_pos = {};
        Vec2U   last_size = {};

        static GDISwapchainData* Get(Swapchain swapchain)
        {
            return static_cast<GDISwapchainData*>(swapchain.operator->());
        }
    };

    Swapchain GDISwapchain_Create(HContext context, Window window, ImageUsage usage);
}