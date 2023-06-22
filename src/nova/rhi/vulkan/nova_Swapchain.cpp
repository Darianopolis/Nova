#include "nova_VulkanContext.hpp"

namespace nova
{
    Swapchain VulkanContext::Swapchain_Create(void* window, TextureUsage usage, PresentMode presentMode)
    {
        auto[id, swapchain] = swapchains.Acquire();
        VkCall(vkCreateWin32SurfaceKHR(instance, Temp(VkWin32SurfaceCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = GetModuleHandle(nullptr),
            .hwnd = HWND(window),
        }), pAlloc, &swapchain.surface));

        swapchain.usage = VkImageUsageFlags(usage)
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapchain.presentMode = VkPresentModeKHR(presentMode);

        std::vector<VkSurfaceFormatKHR> surfaceFormats;
        VkQuery(surfaceFormats, vkGetPhysicalDeviceSurfaceFormatsKHR, gpu, swapchain.surface);

        for (auto& surfaceFormat : surfaceFormats)
        {
            if ((surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM
                || surfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM))
            {
                swapchain.format = surfaceFormat;
                break;
            }
        }

        return id;
    }

    void VulkanContext::Destroy(Swapchain id)
    {
        auto& swapchain = Get(id);

        for (auto semaphore : swapchain.semaphores)
            vkDestroySemaphore(device, semaphore, pAlloc);

        if (swapchain.swapchain)
            vkDestroySwapchainKHR(device, swapchain.swapchain, pAlloc);

        vkDestroySurfaceKHR(instance, swapchain.surface, pAlloc);

        swapchains.Return(id);
    }

    Texture VulkanContext::Swapchain_GetCurrent(Swapchain id)
    {
        auto& swapchain = Get(id);
        return swapchain.textures[swapchain.index];
    }

    Vec2U VulkanContext::Swapchain_GetExtent(Swapchain id)
    {
        auto& swapchain = Get(id);
        return { swapchain.extent.width, swapchain.extent.height };
    }

    Format VulkanContext::Swapchain_GetFormat(Swapchain id)
    {
        return Format(Get(id).format.format);
    }
}