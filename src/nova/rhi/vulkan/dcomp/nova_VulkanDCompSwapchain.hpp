#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>
#include <nova/window/win32/nova_Win32Window.hpp>

#include <nova/core/win32/nova_Win32.hpp>
#include <d3d11_1.h>
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#pragma warning(push)
#pragma warning(disable: 4263)
#include <dcomp.h>
#include <Presentation.h>
#pragma warning(pop)

namespace nova
{
    struct DCompSwapchainBuffer
    {
        // D3D Side
        ID3D11Texture2D*                 texture;
        HANDLE                    texture_handle;
        IPresentationBuffer* presentation_buffer;
        ID3D11Fence*               present_fence;
        HANDLE              present_fence_handle;
        UINT64               present_fence_value;

        // Vulkan Side
        VkDeviceMemory memory;
        Image image;
        Fence fence;

        // State
        std::atomic_uint8_t state;
    };

    struct DCompSwapchainData : Handle<Swapchain>::Impl
    {
        Context context = {};

        Window                 window = {};
        ID3D11Device5*   d3d11_device = {};
        IPresentationManager* manager = {};

        HANDLE            surface_handle = {};
        IPresentationSurface*    surface = {};
        IDCompositionDevice* comp_device = {};
        IDCompositionTarget* comp_target = {};
        IDCompositionVisual* comp_visual = {};
        IUnknown*           comp_surface = {};

        std::atomic_uint8_t   usable_buffer_count;
        u32                          buffer_count;
        u32                          buffer_index;
        std::vector<DCompSwapchainBuffer> buffers;
        std::vector<u32>            present_queue;
        std::vector<HANDLE>                events;
        HANDLE                         lost_event;
        HANDLE                    terminate_event;
        HANDLE                       retire_event;
        ID3D11Fence*                 retire_fence;

        UINT                        width;
        UINT                       height;
        DXGI_FORMAT                format = DXGI_FORMAT_B8G8R8A8_UNORM;
        DXGI_COLOR_SPACE_TYPE color_space = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
        DXGI_ALPHA_MODE        alpha_mode = DXGI_ALPHA_MODE_PREMULTIPLIED;

        std::mutex                          mtx;
        std::atomic_bool present_thread_running;
        std::thread              present_thread;

        ImageUsage usage;

        PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR = nullptr;
        PFN_vkImportSemaphoreWin32HandleKHR     vkImportSemaphoreWin32HandleKHR = nullptr;

        static DCompSwapchainData* Get(Swapchain swapchain)
        {
            return static_cast<DCompSwapchainData*>(swapchain.operator->());
        }
    };

    Swapchain DCompSwapchain_Create(HContext context, Window window, ImageUsage usage, PresentMode present_mode);
}