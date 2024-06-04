#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

#include <nova/core/win32/nova_Win32.hpp>
#include <d3d11_1.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#pragma warning(push)
#pragma warning(disable: 4263)
#include <dcomp.h>
#pragma warning(pop)

namespace nova
{
    struct DXGISwapchainData : Handle<Swapchain>::Impl
    {
        // TODO: This DX state should be centralized >:(
        IDXGIFactory7*    dxfactory = nullptr;
        IDXGIAdapter4*    dxadapter = nullptr;
        ID3D12Device5*     dxdevice = nullptr;
        ID3D12CommandQueue* dxqueue = nullptr;
        IDXGIDevice*    dxgi_device = nullptr;

        bool                 comp_enabled = true;
        IDCompositionDevice* dcomp_device = nullptr;
        IDCompositionVisual* dcomp_visual = nullptr;
        IDCompositionTarget* dcomp_target = nullptr;

        HWND dxhwnd = nullptr;
        IDXGISwapChain4*         dxswapchain = nullptr;
        std::vector<VkDeviceMemory> dxmemory;
        std::vector<HANDLE>        dxhandles;

        PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR = nullptr;

        u32           image_count = 0;
        Format             format = {};
        ImageUsage          usage = {};
        PresentMode  present_mode = PresentMode::Fifo;
        std::vector<Image> images = {};
        uint32_t            index = UINT32_MAX;
        VkExtent2D         extent = { 0, 0 };

        static DXGISwapchainData* Get(Swapchain swapchain)
        {
            return static_cast<DXGISwapchainData*>(swapchain.operator->());
        }
    };

    Swapchain DXGISwapchain_Create(Window window, ImageUsage usage, PresentMode present_mode);
}