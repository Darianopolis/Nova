#include <nova/core/win32/nova_Win32Include.hpp>

#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

namespace nova
{
    PFN_vkGetInstanceProcAddr Platform_LoadGetInstanceProcAddr()
    {
        HMODULE vulkan1 = LoadLibraryA("vulkan-1.dll");

        if (!vulkan1) {
            NOVA_THROW("Failed to load vulkan-1.dll");
        }

        return (PFN_vkGetInstanceProcAddr)(void(*)(void))GetProcAddress(vulkan1, "vkGetInstanceProcAddr");
    }

    void Platform_AddPlatformExtensions(std::vector<const char*>& extensions)
    {
        extensions.push_back("VK_KHR_win32_surface");
    }

    VkSurfaceKHR Platform_CreateVulkanSurface(Context context, void* handle)
    {
        VkSurfaceKHR surface;

        auto vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)
            context->vkGetInstanceProcAddr(context->instance, "vkCreateWin32SurfaceKHR");

        if (!vkCreateWin32SurfaceKHR) {
            NOVA_THROW("Could not load vkCreateWin32SurfaceKHR to create Win32 surface");
        }

        vkh::Check(vkCreateWin32SurfaceKHR(context->instance, Temp(VkWin32SurfaceCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = GetModuleHandle(nullptr),
            .hwnd = HWND(handle),
        }), context->alloc, &surface));

        return surface;
    }
}