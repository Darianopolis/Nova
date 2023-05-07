#include "VulkanBackend.hpp"

namespace pyr
{
    Swapchain Context::CreateSwapchain(VkSurfaceKHR surface, VkImageUsageFlags usage, VkPresentModeKHR presentMode)
    {
        Swapchain swapchain = {};

        std::vector<VkSurfaceFormatKHR> surfaceFormats;
        PYR_VKQUERY(surfaceFormats, vkGetPhysicalDeviceSurfaceFormatsKHR, gpu, surface);

        for (auto& surfaceFormat : surfaceFormats)
        {
            if ((surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM
                || surfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM))
            {
                swapchain.format = surfaceFormat;
                break;
            }
        }

        swapchain.usage = usage;
        swapchain.presentMode = presentMode;
        swapchain.surface = surface;

        return swapchain;
    }

    void Context::DestroySwapchain(Swapchain& swapchain)
    {
        for (auto& image : swapchain.images)
            DestroyImage(image);

        vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);
    }

// -----------------------------------------------------------------------------

    void Context::Present(Swapchain& swapchain)
    {
        Transition(cmd, *swapchain.image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        Flush();

        VkResult result = vkQueuePresentKHR(queue, Temp(VkPresentInfoKHR {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .swapchainCount = 1,
            .pSwapchains = &swapchain.swapchain,
            .pImageIndices = &swapchain.index,
        }));
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            PYR_LOG("Suboptimal / out of date swapchain!\n");
        }
        else
        {
            VkCall(result);
        }
    }

    bool Context::GetNextImage(Swapchain& swapchain)
    {
        VkSurfaceCapabilitiesKHR caps;
        VkCall(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, swapchain.surface, &caps));

        bool resized = !swapchain.swapchain
            || caps.currentExtent.width != swapchain.extent.width
            || caps.currentExtent.height != swapchain.extent.height;

        if (resized)
        {
            VkCall(vkQueueWaitIdle(queue));

            PYR_LOG("Resizing, size = ({}, {})", caps.currentExtent.width, caps.currentExtent.height);

            swapchain.extent = caps.currentExtent;
            VkCall(vkCreateSwapchainKHR(device, Temp(VkSwapchainCreateInfoKHR {
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .surface = swapchain.surface,
                .minImageCount = caps.minImageCount,
                .imageFormat = swapchain.format.format,
                .imageColorSpace = swapchain.format.colorSpace,
                .imageExtent = swapchain.extent,
                .imageArrayLayers = 1,
                .imageUsage = swapchain.usage,
                .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .preTransform = caps.currentTransform,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = swapchain.presentMode,
                .clipped = VK_TRUE,
                .oldSwapchain = swapchain.swapchain,
            }), nullptr, &swapchain.swapchain));

            std::vector<VkImage> vkImages;
            PYR_VKQUERY(vkImages, vkGetSwapchainImagesKHR, device, swapchain.swapchain);

            for (auto& image : swapchain.images)
                DestroyImage(image);

            swapchain.images.resize(vkImages.size());
            for (uint32_t i = 0; i < swapchain.images.size(); ++i)
            {
                auto& image = swapchain.images[i];
                image.image = vkImages[i];
                VkCall(vkCreateImageView(device, Temp(VkImageViewCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = image.image,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = swapchain.format.format,
                    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
                }), nullptr, &image.view));

                image.extent.x = swapchain.extent.width;
                image.extent.y = swapchain.extent.height;
                image.format = swapchain.format.format;
                image.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                image.layout = VK_IMAGE_LAYOUT_UNDEFINED;
                image.mips = 1;
                image.layers = 1;
            }
        }

        VkCall(vkResetFences(device, 1, &fence));
        VkCall(vkAcquireNextImageKHR(device, swapchain.swapchain, UINT64_MAX, VK_NULL_HANDLE, fence, &swapchain.index));
        VkCall(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

        swapchain.image = &swapchain.images[swapchain.index];
        swapchain.image->format = VK_FORMAT_UNDEFINED;

        return resized;
    }
}