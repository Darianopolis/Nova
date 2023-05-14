#include "VulkanBackend.hpp"

namespace pyr
{
    Ref<Swapchain> Context::CreateSwapchain(VkSurfaceKHR surface, VkImageUsageFlags usage, VkPresentModeKHR presentMode)
    {
        Ref swapchain = new Swapchain;
        swapchain->context = this;

        std::vector<VkSurfaceFormatKHR> surfaceFormats;
        PYR_VKQUERY(surfaceFormats, vkGetPhysicalDeviceSurfaceFormatsKHR, gpu, surface);

        for (auto& surfaceFormat : surfaceFormats)
        {
            if ((surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM
                || surfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM))
            {
                swapchain->format = surfaceFormat;
                break;
            }
        }

        swapchain->usage = usage;
        swapchain->presentMode = presentMode;
        swapchain->surface = surface;

        swapchain->semaphores.resize(Swapchain::SemaphoreCount);
        for (auto& s : swapchain->semaphores)
        {
            VkCall(vkCreateSemaphore(device, Temp(VkSemaphoreCreateInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            }), nullptr, &s));
        }

        return swapchain;
    }

    Swapchain::~Swapchain()
    {
        // PYR_LOG("Destroying swapchain");

        for (auto semaphore : semaphores)
            vkDestroySemaphore(context->device, semaphore, nullptr);

        vkDestroySwapchainKHR(context->device, swapchain, nullptr);
    }

// -----------------------------------------------------------------------------

    void Context::Present(Swapchain& swapchain, Semaphore* wait)
    {
        if (wait)
        {
            VkCall(vkQueueSubmit2(graphics.handle, 1, Temp(VkSubmitInfo2 {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = 1,
                .pWaitSemaphoreInfos = Temp(VkSemaphoreSubmitInfo {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = semaphore->semaphore,
                    .value = semaphore->value,
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                }),
                .signalSemaphoreInfoCount = 1,
                .pSignalSemaphoreInfos = Temp(VkSemaphoreSubmitInfo {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = swapchain.semaphores[swapchain.semaphoreIndex],
                }),
            }), nullptr));
        }

        VkResult result = vkQueuePresentKHR(graphics.handle, Temp(VkPresentInfoKHR {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = wait ? 1u : 0u,
            .pWaitSemaphores = wait ? &swapchain.semaphores[swapchain.semaphoreIndex] : nullptr,
            .swapchainCount = 1,
            .pSwapchains = &swapchain.swapchain,
            .pImageIndices = &swapchain.index,
        }));

        if (wait)
            swapchain.semaphoreIndex = (swapchain.semaphoreIndex + 1) % swapchain.semaphores.size();

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            PYR_LOG("Suboptimal / out of date swapchain!\n");
        }
        else
        {
            VkCall(result);
        }
    }

    bool Context::GetNextImage(Swapchain& swapchain, Queue& queue, Semaphore* signal)
    {
        VkSurfaceCapabilitiesKHR caps;
        VkCall(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, swapchain.surface, &caps));

        bool resized = !swapchain.swapchain
            || caps.currentExtent.width != swapchain.extent.width
            || caps.currentExtent.height != swapchain.extent.height;

        if (resized)
        {
            VkCall(vkQueueWaitIdle(graphics.handle));

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
                .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, // TODO: concurrent for async present
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = &queue.family,
                .preTransform = caps.currentTransform,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = swapchain.presentMode,
                .clipped = VK_TRUE,
                .oldSwapchain = swapchain.swapchain,
            }), nullptr, &swapchain.swapchain));

            std::vector<VkImage> vkImages;
            PYR_VKQUERY(vkImages, vkGetSwapchainImagesKHR, device, swapchain.swapchain);

            swapchain.images.resize(vkImages.size());
            for (uint32_t i = 0; i < swapchain.images.size(); ++i)
            {
                auto& image = swapchain.images[i];
                image = new Image;
                image->context = this;
                image->image = vkImages[i];
                VkCall(vkCreateImageView(device, Temp(VkImageViewCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = image->image,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = swapchain.format.format,
                    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
                }), nullptr, &image->view));

                image->extent.x = swapchain.extent.width;
                image->extent.y = swapchain.extent.height;
                image->format = swapchain.format.format;
                image->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                image->layout = VK_IMAGE_LAYOUT_UNDEFINED;
                image->mips = 1;
                image->layers = 1;
            }
        }

        VkCall(vkAcquireNextImageKHR(device, swapchain.swapchain, UINT64_MAX,
            signal ? swapchain.semaphores[swapchain.semaphoreIndex] : nullptr, nullptr, &swapchain.index));

        if (signal)
        {
            VkCall(vkQueueSubmit2(queue.handle, 1, Temp(VkSubmitInfo2 {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = 1,
                .pWaitSemaphoreInfos = Temp(VkSemaphoreSubmitInfo {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = swapchain.semaphores[swapchain.semaphoreIndex],
                }),
                .signalSemaphoreInfoCount = 1,
                .pSignalSemaphoreInfos = Temp(VkSemaphoreSubmitInfo {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = signal->semaphore,
                    .value = ++semaphore->value,
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                }),
            }), nullptr));

            swapchain.semaphoreIndex = (swapchain.semaphoreIndex + 1) % swapchain.semaphores.size();
        }

        swapchain.image = swapchain.images[swapchain.index].Raw();
        swapchain.image->format = VK_FORMAT_UNDEFINED;

        return resized;
    }
}