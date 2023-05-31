#include "nova_RHI.hpp"

namespace nova
{
    Rc<Swapchain> Context::CreateSwapchain(Rc<Surface> surface, TextureUsage _usage, PresentMode _presentMode)
    {
        Rc swapchain = new Swapchain;
        swapchain->surface = surface;

        auto usage = VkImageUsageFlags(_usage);
        auto presentMode = VkPresentModeKHR(_presentMode);

        std::vector<VkSurfaceFormatKHR> surfaceFormats;
        VkQuery(surfaceFormats, vkGetPhysicalDeviceSurfaceFormatsKHR, gpu, surface->surface);

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

        return swapchain;
    }

    Swapchain::~Swapchain()
    {
        auto* context = surface->context.Get();

        for (auto semaphore : semaphores)
            vkDestroySemaphore(context->device, semaphore, context->pAlloc);

        vkDestroySwapchainKHR(context->device, swapchain, context->pAlloc);
    }

// -----------------------------------------------------------------------------

    void CommandList::Present(Texture* texture)
    {
        Transition(texture, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_NONE, 0);
    }

    NOVA_NO_INLINE
    void Queue::Present(Span<Swapchain*> swapchains, Span<Fence*> waits, bool hostWait)
    {
        VkSemaphore* binaryWaits = nullptr;

        if (hostWait && waits.size())
        {
            auto semaphores = NOVA_ALLOC_STACK(VkSemaphore, waits.size());
            auto values = NOVA_ALLOC_STACK(u64, waits.size());
            for (u32 i = 0; i < waits.size(); ++i)
            {
                semaphores[i] = waits[i]->semaphore;
                values[i] = waits[i]->value;
            }

            VkCall(vkWaitSemaphores(context->device, Temp(VkSemaphoreWaitInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                .semaphoreCount = u32(waits.size()),
                .pSemaphores = semaphores,
                .pValues = values,
            }), UINT64_MAX));
        }
        else
        if (waits.size())
        {
            auto waitInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, waits.size());
            for (u32 i = 0; i < waits.size(); ++i)
            {
                waitInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = waits[i]->semaphore,
                    .value = waits[i]->value,
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                };
            }

            auto signalInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, swapchains.size());
            for (u32 i = 0; i < swapchains.size(); ++i)
            {
                auto swapchain = swapchains[i];
                signalInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = swapchain->semaphores[swapchain->semaphoreIndex],
                };
            }

            auto start = std::chrono::steady_clock::now();
            VkCall(vkQueueSubmit2(handle, 1, Temp(VkSubmitInfo2 {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = u32(waits.size()),
                .pWaitSemaphoreInfos = waitInfos,
                .signalSemaphoreInfoCount = u32(swapchains.size()),
                .pSignalSemaphoreInfos = signalInfos,
            }), nullptr));
            adapting2 += std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now() - start).count();

            binaryWaits = NOVA_ALLOC_STACK(VkSemaphore, swapchains.size());
            for (u32 i = 0; i < swapchains.size(); ++i)
            {
                auto swapchain = swapchains[i];
                binaryWaits[i] = swapchain->semaphores[swapchain->semaphoreIndex];
                swapchain->semaphoreIndex = (swapchain->semaphoreIndex + 1) % swapchain->semaphores.size();
            }
        }

        auto vkSwapchains = NOVA_ALLOC_STACK(VkSwapchainKHR, swapchains.size());
        auto indices = NOVA_ALLOC_STACK(u32, swapchains.size());
        for (u32 i = 0; i < swapchains.size(); ++i)
        {
            auto swapchain = swapchains[i];
            vkSwapchains[i] = swapchain->swapchain;
            indices[i] = swapchain->index;
        }

        auto results = NOVA_ALLOC_STACK(VkResult, swapchains.size());
        auto start = std::chrono::steady_clock::now();
        vkQueuePresentKHR(handle, Temp(VkPresentInfoKHR {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = binaryWaits ? u32(swapchains.size()) : 0u,
            .pWaitSemaphores = binaryWaits,
            .swapchainCount = u32(swapchains.size()),
            .pSwapchains = vkSwapchains,
            .pImageIndices = indices,
            .pResults = results,
        }));
        presenting += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();

        for (u32 i = 0; i < swapchains.size(); ++i)
        {
            if (results[i] == VK_ERROR_OUT_OF_DATE_KHR || results[i] == VK_SUBOPTIMAL_KHR)
            {
                NOVA_LOG("Swapchain[{}] present returned out-of-date/suboptimal ({})", (void*)swapchains[i], int(results[i]));
                swapchains[i]->invalid = true;
            }
            else
            {
                VkCall(results[i]);
            }
        }
    }

    NOVA_NO_INLINE
    bool Queue::Acquire(Span<Swapchain*> swapchains, Span<Fence*> signals)
    {
        bool anyResized = false;

        for (auto swapchain : swapchains)
        {
            do
            {
                VkSurfaceCapabilitiesKHR caps;
                VkCall(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->gpu, swapchain->surface->surface, &caps));

                bool recreate = swapchain->invalid
                    || !swapchain->swapchain
                    || caps.currentExtent.width != swapchain->extent.width
                    || caps.currentExtent.height != swapchain->extent.height;

                if (recreate)
                {
                    anyResized |= recreate;

                    VkCall(vkQueueWaitIdle(handle));

                    auto oldSwapchain = swapchain->swapchain;

                    swapchain->extent = caps.currentExtent;
                    VkCall(vkCreateSwapchainKHR(context->device, Temp(VkSwapchainCreateInfoKHR {
                        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                        .surface = swapchain->surface->surface,
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

                    swapchain->invalid = false;

                    vkDestroySwapchainKHR(context->device, oldSwapchain, context->pAlloc);

                    std::vector<VkImage> vkImages;
                    VkQuery(vkImages, vkGetSwapchainImagesKHR, context->device, swapchain->swapchain);

                    while (swapchain->semaphores.size() < vkImages.size() * 2)
                    {
                        auto& semaphore = swapchain->semaphores.emplace_back();
                        VkCall(vkCreateSemaphore(context->device, Temp(VkSemaphoreCreateInfo {
                            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                        }), context->pAlloc, &semaphore));
                    }

                    swapchain->textures.clear();
                    swapchain->textures.resize(vkImages.size());

                    for (uint32_t i = 0; i < swapchain->textures.size(); ++i)
                    {
                        auto& texture = swapchain->textures[i];
                        texture = new Texture;
                        texture->context = context;
                        texture->image = vkImages[i];

                        VkCall(vkCreateImageView(context->device, Temp(VkImageViewCreateInfo {
                            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                            .image = texture->image,
                            .viewType = VK_IMAGE_VIEW_TYPE_2D,
                            .format = swapchain->format.format,
                            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
                        }), context->pAlloc, &texture->view));

                        texture->extent.x = swapchain->extent.width;
                        texture->extent.y = swapchain->extent.height;
                        texture->format = swapchain->format.format;
                        texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                        texture->mips = 1;
                        texture->layers = 1;
                    }
                }

                auto result = vkAcquireNextImage2KHR(context->device, Temp(VkAcquireNextImageInfoKHR {
                    .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
                    .swapchain = swapchain->swapchain,
                    .timeout = UINT64_MAX,
                    .semaphore = signals.size() ? swapchain->semaphores[swapchain->semaphoreIndex] : nullptr,
                    .deviceMask = 1,
                }), &swapchain->index);

                if (result == VK_SUCCESS)
                {
                    swapchain->current = swapchain->textures[swapchain->index];
                }
                else
                if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
                {
                    NOVA_LOG("Swapchain[{}] acquire returned out-of-date/suboptimal ({})", (void*)swapchain, int(result));
                    swapchain->invalid = true;
                }
                else
                {
                    VkCall(result);
                }
            }
            while (swapchain->invalid);
        }

        if (signals.size())
        {
            auto waitInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, swapchains.size());
            for (u32 i = 0; i < swapchains.size(); ++i)
            {
                auto swapchain = swapchains[i];
                waitInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = swapchain->semaphores[swapchain->semaphoreIndex],
                };
                swapchain->semaphoreIndex = (swapchain->semaphoreIndex + 1) % swapchain->semaphores.size();
            }

            auto signalInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, signals.size());
            for (u32 i = 0; i < signals.size(); ++i)
            {
                auto signal = signals[i];
                signalInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = signal->semaphore,
                    .value = ++signal->value,
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                };
            }

            auto start = std::chrono::steady_clock::now();
            VkCall(vkQueueSubmit2(handle, 1, Temp(VkSubmitInfo2 {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = u32(swapchains.size()),
                .pWaitSemaphoreInfos = waitInfos,
                .signalSemaphoreInfoCount = u32(signals.size()),
                .pSignalSemaphoreInfos = signalInfos,
            }), nullptr));
            adapting1 += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
        }

        return anyResized;
    }

    Rc<Surface> Context::CreateSurface(HWND hwnd)
    {
        Rc surface = new Surface;
        surface->context = this;

        VkCall(vkCreateWin32SurfaceKHR(instance, Temp(VkWin32SurfaceCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = GetModuleHandle(nullptr),
            .hwnd = hwnd,
        }), pAlloc, &surface->surface));

        return surface;
    }

    Surface::~Surface()
    {
        vkDestroySurfaceKHR(context->instance, surface, context->pAlloc);
    }
}