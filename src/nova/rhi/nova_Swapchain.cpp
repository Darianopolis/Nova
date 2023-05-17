#include "nova_RHI.hpp"

namespace nova
{
    Swapchain* Context::CreateSwapchain(VkSurfaceKHR surface, VkImageUsageFlags usage, VkPresentModeKHR presentMode)
    {
        auto swapchain = new Swapchain;
        swapchain->context = this;

        std::vector<VkSurfaceFormatKHR> surfaceFormats;
        VkQuery(surfaceFormats, vkGetPhysicalDeviceSurfaceFormatsKHR, gpu, surface);

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
            }), pAlloc, &s));
        }

        return swapchain;
    }

    void Context::Destroy(Swapchain* swapchain)
    {
        for (auto semaphore : swapchain->semaphores)
            vkDestroySemaphore(device, semaphore, pAlloc);

        for (auto image : swapchain->images)
            DestroyImage(image);

        vkDestroySwapchainKHR(device, swapchain->swapchain, pAlloc);

        delete swapchain;
    }

// -----------------------------------------------------------------------------

    NOVA_NO_INLINE
    void Queue::Present(std::initializer_list<Swapchain*> swapchains, std::initializer_list<Fence*> waits)
    {
        VkSemaphore* binaryWaits = nullptr;

        if (waits.size())
        {
            auto waitInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, waits.size());
            for (u32 i = 0; i < waits.size(); ++i)
            {
                waitInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = waits.begin()[i]->semaphore,
                    .value = waits.begin()[i]->value,
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                };
            }

            auto signalInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, swapchains.size());
            for (u32 i = 0; i < swapchains.size(); ++i)
            {
                auto swapchain = swapchains.begin()[i];
                signalInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = swapchain->semaphores[swapchain->semaphoreIndex],
                };
            }

            VkCall(vkQueueSubmit2(handle, 1, Temp(VkSubmitInfo2 {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = u32(waits.size()),
                .pWaitSemaphoreInfos = waitInfos,
                .signalSemaphoreInfoCount = u32(swapchains.size()),
                .pSignalSemaphoreInfos = signalInfos,
            }), nullptr));

            binaryWaits = NOVA_ALLOC_STACK(VkSemaphore, swapchains.size());
            for (u32 i = 0; i < swapchains.size(); ++i)
            {
                auto swapchain = swapchains.begin()[i];
                binaryWaits[i] = swapchain->semaphores[swapchain->semaphoreIndex];
                swapchain->semaphoreIndex = (swapchain->semaphoreIndex + 1) % swapchain->semaphores.size();
            }
        }

        auto vkSwapchains = NOVA_ALLOC_STACK(VkSwapchainKHR, swapchains.size());
        auto indices = NOVA_ALLOC_STACK(u32, swapchains.size());
        for (u32 i = 0; i < swapchains.size(); ++i)
        {
            auto swapchain = swapchains.begin()[i];
            vkSwapchains[i] = swapchain->swapchain;
            indices[i] = swapchain->index;
        }

        // std::this_thread::sleep_for(200ms);
        auto result = vkQueuePresentKHR(handle, Temp(VkPresentInfoKHR {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = waits.size() ? u32(swapchains.size()) : 0u,
            .pWaitSemaphores = binaryWaits,
            .swapchainCount = u32(swapchains.size()),
            .pSwapchains = vkSwapchains,
            .pImageIndices = indices,
        }));

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            NOVA_LOG("Suboptimal / out of date swapchain!\n");
        }
        else
        {
            VkCall(result);
        }
    }

    NOVA_NO_INLINE
    bool Queue::Acquire(std::initializer_list<Swapchain*> swapchains, std::initializer_list<Fence*> signals)
    {
        bool anyResized = false;

        for (auto swapchain : swapchains)
        {
            VkSurfaceCapabilitiesKHR caps;
            VkCall(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->gpu, swapchain->surface, &caps));

            bool resized = !swapchain->swapchain
                || caps.currentExtent.width != swapchain->extent.width
                || caps.currentExtent.height != swapchain->extent.height;

            if (resized)
            {
                anyResized |= resized;

                VkCall(vkQueueWaitIdle(handle));

                auto oldSwapchain = swapchain->swapchain;

                swapchain->extent = caps.currentExtent;
                VkCall(vkCreateSwapchainKHR(context->device, Temp(VkSwapchainCreateInfoKHR {
                    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                    .surface = swapchain->surface,
                    .minImageCount = caps.minImageCount,
                    .imageFormat = swapchain->format.format,
                    .imageColorSpace = swapchain->format.colorSpace,
                    .imageExtent = swapchain->extent,
                    .imageArrayLayers = 1,
                    .imageUsage = swapchain->usage,
                    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, // TODO: concurrent for async present
                    .queueFamilyIndexCount = 1,
                    .pQueueFamilyIndices = &family,
                    .preTransform = caps.currentTransform,
                    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                    .presentMode = swapchain->presentMode,
                    .clipped = VK_TRUE,
                    .oldSwapchain = oldSwapchain,
                }), context->pAlloc, &swapchain->swapchain));

                vkDestroySwapchainKHR(context->device, oldSwapchain, context->pAlloc);

                std::vector<VkImage> vkImages;
                VkQuery(vkImages, vkGetSwapchainImagesKHR, context->device, swapchain->swapchain);

                for (auto image : swapchain->images)
                    context->DestroyImage(image);

                swapchain->images.resize(vkImages.size());
                for (uint32_t i = 0; i < swapchain->images.size(); ++i)
                {
                    auto& image = swapchain->images[i];
                    image = new Image;
                    image->context = context;
                    image->image = vkImages[i];
                    VkCall(vkCreateImageView(context->device, Temp(VkImageViewCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                        .image = image->image,
                        .viewType = VK_IMAGE_VIEW_TYPE_2D,
                        .format = swapchain->format.format,
                        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
                    }), context->pAlloc, &image->view));

                    image->extent.x = swapchain->extent.width;
                    image->extent.y = swapchain->extent.height;
                    image->format = swapchain->format.format;
                    image->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                    image->layout = VK_IMAGE_LAYOUT_UNDEFINED;
                    image->mips = 1;
                    image->layers = 1;
                }
            }

            VkCall(vkAcquireNextImageKHR(context->device, swapchain->swapchain, UINT64_MAX,
                signals.size() ? swapchain->semaphores[swapchain->semaphoreIndex] : nullptr, nullptr, &swapchain->index));

            swapchain->image = swapchain->images[swapchain->index];
            swapchain->image->format = VK_FORMAT_UNDEFINED;
        }

        if (signals.size())
        {
            auto waitInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, swapchains.size());
            for (u32 i = 0; i < swapchains.size(); ++i)
            {
                auto swapchain = swapchains.begin()[i];
                waitInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = swapchain->semaphores[swapchain->semaphoreIndex],
                };
                swapchain->semaphoreIndex = (swapchain->semaphoreIndex + 1) % swapchain->semaphores.size();
            }

            auto signalInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, signals.size());
            for (u32 i = 0; i < signals.size(); ++i)
            {
                auto signal = signals.begin()[i];
                signalInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = signal->semaphore,
                    .value = ++signal->value,
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                };
            }

            VkCall(vkQueueSubmit2(handle, 1, Temp(VkSubmitInfo2 {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = u32(swapchains.size()),
                .pWaitSemaphoreInfos = waitInfos,
                .signalSemaphoreInfoCount = u32(signals.size()),
                .pSignalSemaphoreInfos = signalInfos,
            }), nullptr));

        }

        return anyResized;
    }

    VkSurfaceKHR Context::CreateSurface(HWND hwnd)
    {
        VkSurfaceKHR surface;
        VkCall(vkCreateWin32SurfaceKHR(instance, Temp(VkWin32SurfaceCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = GetModuleHandle(nullptr),
            .hwnd = hwnd,
        }), pAlloc, &surface));

        return surface;
    }

    void Context::Destroy(VkSurfaceKHR surface)
    {
        vkDestroySurfaceKHR(instance, surface, pAlloc);
    }
}