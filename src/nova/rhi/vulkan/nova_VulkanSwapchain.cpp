#include "nova_VulkanRHI.hpp"

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

    void VulkanContext::Swapchain_Destroy(Swapchain id)
    {
        auto& swapchain = Get(id);

        for (auto semaphore : swapchain.semaphores)
            vkDestroySemaphore(device, semaphore, pAlloc);

        for (auto texture : swapchain.textures)
            Texture_Destroy(texture);

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

// -----------------------------------------------------------------------------

    bool VulkanContext::Queue_Acquire(Queue hQueue, Span<Swapchain> _swapchains, Span<Fence> signals)
    {
        auto& queue = Get(hQueue);

        bool anyResized = false;

        for (auto hSwapchain : _swapchains)
        {
            auto& swapchain = Get(hSwapchain);

            do
            {
                VkSurfaceCapabilitiesKHR caps;
                VkCall(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, swapchain.surface, &caps));

                bool recreate = swapchain.invalid
                    || !swapchain.swapchain
                    || caps.currentExtent.width != swapchain.extent.width
                    || caps.currentExtent.height != swapchain.extent.height;

                if (recreate)
                {
                    anyResized |= recreate;

                    VkCall(vkQueueWaitIdle(queue.handle));

                    auto oldSwapchain = swapchain.swapchain;

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
                        .oldSwapchain = oldSwapchain,
                    }), pAlloc, &swapchain.swapchain));

                    swapchain.invalid = false;

                    vkDestroySwapchainKHR(device, oldSwapchain, pAlloc);

                    std::vector<VkImage> vkImages;
                    VkQuery(vkImages, vkGetSwapchainImagesKHR, device, swapchain.swapchain);

                    while (swapchain.semaphores.size() < vkImages.size() * 2)
                    {
                        auto& semaphore = swapchain.semaphores.emplace_back();
                        VkCall(vkCreateSemaphore(device, Temp(VkSemaphoreCreateInfo {
                            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                        }), pAlloc, &semaphore));
                    }

                    for (auto texture : swapchain.textures)
                        Texture_Destroy(texture);

                    swapchain.textures.resize(vkImages.size());
                    for (uint32_t i = 0; i < vkImages.size(); ++i)
                    {
                        auto& hTexture = swapchain.textures[i];
                        hTexture = textures.Acquire().first;
                        auto& texture = Get(hTexture);

                        texture.allocation = nullptr;
                        texture.image = vkImages[i];
                        texture.usage = swapchain.usage;

                        VkCall(vkCreateImageView(device, Temp(VkImageViewCreateInfo {
                            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                            .image = texture.image,
                            .viewType = VK_IMAGE_VIEW_TYPE_2D,
                            .format = swapchain.format.format,
                            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
                        }), pAlloc, &texture.view));

                        texture.extent.x = swapchain.extent.width;
                        texture.extent.y = swapchain.extent.height;
                        texture.format = swapchain.format.format;
                        texture.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                        texture.mips = 1;
                        texture.layers = 1;
                    }
                }

                auto result = vkAcquireNextImage2KHR(device, Temp(VkAcquireNextImageInfoKHR {
                    .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
                    .swapchain = swapchain.swapchain,
                    .timeout = UINT64_MAX,
                    .semaphore = signals.size() ? swapchain.semaphores[swapchain.semaphoreIndex] : nullptr,
                    .deviceMask = 1,
                }), &swapchain.index);

                if (result != VK_SUCCESS)
                {
                    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
                    {
                        NOVA_LOG("Swapchain[{}] acquire returned out-of-date/suboptimal ({})", (void*)swapchain.swapchain, int(result));
                        swapchain.invalid = true;
                    }
                    else
                    {
                        VkCall(result);
                    }
                }
            }
            while (swapchain.invalid);
        }

        if (signals.size())
        {
            auto waitInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, _swapchains.size());
            for (u32 i = 0; i < _swapchains.size(); ++i)
            {
                auto swapchain = Get(_swapchains[i]);
                waitInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = swapchain.semaphores[swapchain.semaphoreIndex],
                };
                swapchain.semaphoreIndex = (swapchain.semaphoreIndex + 1) % swapchain.semaphores.size();
            }

            auto signalInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, signals.size());
            for (u32 i = 0; i < signals.size(); ++i)
            {
                auto signal = Get(signals[i]);
                signalInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = signal.semaphore,
                    .value = Fence_Advance(signals[i]),
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                };
            }

            auto start = std::chrono::steady_clock::now();
            VkCall(vkQueueSubmit2(queue.handle, 1, Temp(VkSubmitInfo2 {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = u32(_swapchains.size()),
                .pWaitSemaphoreInfos = waitInfos,
                .signalSemaphoreInfoCount = u32(signals.size()),
                .pSignalSemaphoreInfos = signalInfos,
            }), nullptr));
            TimeAdaptingFromAcquire += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        }

        return anyResized;
    }

    void VulkanContext::Queue_Present(Queue hQueue, Span<Swapchain> _swapchains, Span<Fence> waits, bool hostWait)
    {
        auto& queue = Get(hQueue);

        VkSemaphore* binaryWaits = nullptr;

        if (hostWait && waits.size())
        {
            auto semaphores = NOVA_ALLOC_STACK(VkSemaphore, waits.size());
            auto values = NOVA_ALLOC_STACK(u64, waits.size());
            for (u32 i = 0; i < waits.size(); ++i)
            {
                auto& wait = Get(waits[i]);
                semaphores[i] = wait.semaphore;
                values[i] = wait.value;
            }

            VkCall(vkWaitSemaphores(device, Temp(VkSemaphoreWaitInfo {
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
                auto& wait = Get(waits[i]);
                waitInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = wait.semaphore,
                    .value = wait.value,
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                };
            }

            auto signalInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, _swapchains.size());
            for (u32 i = 0; i < _swapchains.size(); ++i)
            {
                auto& swapchain = Get(_swapchains[i]);
                signalInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = swapchain.semaphores[swapchain.semaphoreIndex],
                };
            }

            auto start = std::chrono::steady_clock::now();
            VkCall(vkQueueSubmit2(queue.handle, 1, Temp(VkSubmitInfo2 {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = u32(waits.size()),
                .pWaitSemaphoreInfos = waitInfos,
                .signalSemaphoreInfoCount = u32(_swapchains.size()),
                .pSignalSemaphoreInfos = signalInfos,
            }), nullptr));
            TimeAdaptingToPresent += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();

            binaryWaits = NOVA_ALLOC_STACK(VkSemaphore, _swapchains.size());
            for (u32 i = 0; i < _swapchains.size(); ++i)
            {
                auto swapchain = Get(_swapchains[i]);
                binaryWaits[i] = swapchain.semaphores[swapchain.semaphoreIndex];
                swapchain.semaphoreIndex = (swapchain.semaphoreIndex + 1) % swapchain.semaphores.size();
            }
        }

        auto vkSwapchains = NOVA_ALLOC_STACK(VkSwapchainKHR, _swapchains.size());
        auto indices = NOVA_ALLOC_STACK(u32, _swapchains.size());
        for (u32 i = 0; i < _swapchains.size(); ++i)
        {
            auto swapchain = Get(_swapchains[i]);
            vkSwapchains[i] = swapchain.swapchain;
            indices[i] = swapchain.index;
        }

        auto results = NOVA_ALLOC_STACK(VkResult, _swapchains.size());
        auto start = std::chrono::steady_clock::now();
        vkQueuePresentKHR(queue.handle, Temp(VkPresentInfoKHR {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = binaryWaits ? u32(_swapchains.size()) : 0u,
            .pWaitSemaphores = binaryWaits,
            .swapchainCount = u32(_swapchains.size()),
            .pSwapchains = vkSwapchains,
            .pImageIndices = indices,
            .pResults = results,
        }));
        TimePresenting += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();

        for (u32 i = 0; i < _swapchains.size(); ++i)
        {
            if (results[i] == VK_ERROR_OUT_OF_DATE_KHR || results[i] == VK_SUBOPTIMAL_KHR)
            {
                NOVA_LOG("Swapchain[{}] present returned out-of-date/suboptimal ({})", (void*)Get(_swapchains[i]).swapchain, int(results[i]));
                Get(_swapchains[i]).invalid = true;
            }
            else
            {
                VkCall(results[i]);
            }
        }
    }

    void VulkanContext::Cmd_Present(CommandList cmd, Swapchain swapchain)
    {
        Cmd_Transition(cmd, Swapchain_GetCurrent(swapchain),
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_2_NONE,
            0);
    }
}