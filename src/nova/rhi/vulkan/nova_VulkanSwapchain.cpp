#include "nova_VulkanRHI.hpp"

namespace nova
{
    Swapchain Swapchain::Create(HContext context, void* window, TextureUsage usage, PresentMode presentMode)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->usage = usage;
        impl->presentMode = presentMode;

        VkCall(vkCreateWin32SurfaceKHR(context->instance, Temp(VkWin32SurfaceCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = GetModuleHandle(nullptr),
            .hwnd = HWND(window),
        }), context->pAlloc, &impl->surface));

        std::vector<VkSurfaceFormatKHR> surfaceFormats;
        VkQuery(surfaceFormats, vkGetPhysicalDeviceSurfaceFormatsKHR, context->gpu, impl->surface);

        for (auto& surfaceFormat : surfaceFormats) {
            if ((surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM
                    || surfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM)) {
                impl->format = surfaceFormat;
                break;
            }
        }

        return { impl };
    }

    void Swapchain::Destroy()
    {
        if (!impl) {
            return;
        }
        
        for (auto semaphore : impl->semaphores) {
            vkDestroySemaphore(impl->context->device, semaphore, impl->context->pAlloc);
        }

        for (auto& texture : impl->textures) {
            texture.Destroy();
        }

        if (impl->swapchain) {
            vkDestroySwapchainKHR(impl->context->device, impl->swapchain, impl->context->pAlloc);
        }

        vkDestroySurfaceKHR(impl->context->instance, impl->surface, impl->context->pAlloc);

        delete impl;
        impl = nullptr;
    }

    Texture Swapchain::GetCurrent() const
    {
        return impl->textures[impl->index];
    }

    Vec2U Swapchain::GetExtent() const
    {
        return { impl->extent.width, impl->extent.height };
    }

    Format Swapchain::GetFormat() const
    {
        return FromVulkanFormat(impl->format.format);
    }

// -----------------------------------------------------------------------------

    bool Queue::Acquire(Span<HSwapchain> _swapchains, Span<HFence> signals) const
    {
        bool anyResized = false;

        for (auto swapchain : _swapchains) {
            do {
                VkSurfaceCapabilitiesKHR caps;
                VkCall(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(impl->context->gpu, swapchain->surface, &caps));

                bool recreate = swapchain->invalid
                    || !swapchain->swapchain
                    || caps.currentExtent.width != swapchain->extent.width
                    || caps.currentExtent.height != swapchain->extent.height;

                if (recreate) {
                    anyResized |= recreate;

                    VkCall(vkQueueWaitIdle(impl->handle));

                    auto oldSwapchain = swapchain->swapchain;

                    auto vkUsage = GetVulkanImageUsage(swapchain->usage)
                        | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

                    swapchain->extent = caps.currentExtent;
                    VkCall(vkCreateSwapchainKHR(impl->context->device, Temp(VkSwapchainCreateInfoKHR {
                        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                        .surface = swapchain->surface,
                        .minImageCount = caps.minImageCount,
                        .imageFormat = swapchain->format.format,
                        .imageColorSpace = swapchain->format.colorSpace,
                        .imageExtent = swapchain->extent,
                        .imageArrayLayers = 1,
                        .imageUsage = vkUsage,
                        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, // TODO: concurrent for async present
                        .queueFamilyIndexCount = 1,
                        .pQueueFamilyIndices = &impl->family,
                        .preTransform = caps.currentTransform,
                        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                        .presentMode = GetVulkanPresentMode(swapchain->presentMode),
                        .clipped = VK_TRUE,
                        .oldSwapchain = oldSwapchain,
                    }), impl->context->pAlloc, &swapchain->swapchain));

                    swapchain->semaphoreIndex = false;

                    vkDestroySwapchainKHR(impl->context->device, oldSwapchain, impl->context->pAlloc);

                    std::vector<VkImage> vkImages;
                    VkQuery(vkImages, vkGetSwapchainImagesKHR, impl->context->device, swapchain->swapchain);

                    while (swapchain->semaphores.size() < vkImages.size() * 2) {
                        auto& semaphore = swapchain->semaphores.emplace_back();
                        VkCall(vkCreateSemaphore(impl->context->device, Temp(VkSemaphoreCreateInfo {
                            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                        }), impl->context->pAlloc, &semaphore));
                    }

                    for (auto& texture : swapchain->textures) {
                        texture.Destroy();
                    }
                    swapchain->textures.resize(vkImages.size());
                    for (uint32_t i = 0; i < vkImages.size(); ++i) {
                        auto& texture = (swapchain->textures[i] = { new Texture::Impl });
                        texture->context = impl->context;

                        texture->usage = swapchain->usage;
                        texture->format = swapchain().GetFormat();
                        texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                        texture->extent = Vec3(swapchain->extent.width, swapchain->extent.height, 1);
                        texture->mips = 1;
                        texture->layers = 1;

                        texture->image = vkImages[i];
                        VkCall(vkCreateImageView(impl->context->device, Temp(VkImageViewCreateInfo {
                            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                            .image = vkImages[i],
                            .viewType = VK_IMAGE_VIEW_TYPE_2D,
                            .format = swapchain->format.format,
                            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
                        }), impl->context->pAlloc, &texture->view));
                    }
                }

                auto result = vkAcquireNextImage2KHR(impl->context->device, Temp(VkAcquireNextImageInfoKHR {
                    .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
                    .swapchain = swapchain->swapchain,
                    .timeout = UINT64_MAX,
                    .semaphore = signals.size() ? swapchain->semaphores[swapchain->semaphoreIndex] : nullptr,
                    .deviceMask = 1,
                }), &swapchain->index);

                if (result != VK_SUCCESS) {
                    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
                        NOVA_LOG("Swapchain[{}] acquire returned out-of-date/suboptimal ({})", (void*)swapchain->swapchain, int(result));
                        swapchain->invalid = true;
                    } else {
                        VkCall(result);
                    }
                }
            } while (swapchain->invalid);
        }

        if (signals.size())
        {
            auto waitInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, _swapchains.size());
            for (u32 i = 0; i < _swapchains.size(); ++i) {
                auto swapchain = _swapchains[i];
                waitInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = swapchain->semaphores[swapchain->semaphoreIndex],
                };
                swapchain->semaphoreIndex = (swapchain->semaphoreIndex + 1) % swapchain->semaphores.size();
            }

            auto signalInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, signals.size());
            for (u32 i = 0; i < signals.size(); ++i) {
                auto signal = signals[i];
                signalInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = signal->semaphore,
                    .value = signals[i]().Advance(),
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                };
            }

            auto start = std::chrono::steady_clock::now();
            VkCall(vkQueueSubmit2(impl->handle, 1, Temp(VkSubmitInfo2 {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = u32(_swapchains.size()),
                .pWaitSemaphoreInfos = waitInfos,
                .signalSemaphoreInfoCount = u32(signals.size()),
                .pSignalSemaphoreInfos = signalInfos,
            }), nullptr));
            rhi::stats::TimeAdaptingFromAcquire += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        }

        return anyResized;
    }

    void Queue::Present(Span<HSwapchain> _swapchains, Span<HFence> waits, bool hostWait) const
    {
        VkSemaphore* binaryWaits = nullptr;

        if (hostWait && waits.size()) {
            auto semaphores = NOVA_ALLOC_STACK(VkSemaphore, waits.size());
            auto values = NOVA_ALLOC_STACK(u64, waits.size());
            for (u32 i = 0; i < waits.size(); ++i) {
                semaphores[i] = waits[i]->semaphore;
                values[i] = waits[i]->value;
            }

            VkCall(vkWaitSemaphores(impl->context->device, Temp(VkSemaphoreWaitInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                .semaphoreCount = u32(waits.size()),
                .pSemaphores = semaphores,
                .pValues = values,
            }), UINT64_MAX));

        } else if (waits.size()) {
            auto waitInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, waits.size());
            for (u32 i = 0; i < waits.size(); ++i) {
                waitInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = waits[i]->semaphore,
                    .value = waits[i]->value,
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                };
            }

            auto signalInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, _swapchains.size());
            for (u32 i = 0; i < _swapchains.size(); ++i) {
                auto swapchain = _swapchains[i];
                signalInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = swapchain->semaphores[swapchain->semaphoreIndex],
                };
            }

            auto start = std::chrono::steady_clock::now();
            VkCall(vkQueueSubmit2(impl->handle, 1, Temp(VkSubmitInfo2 {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = u32(waits.size()),
                .pWaitSemaphoreInfos = waitInfos,
                .signalSemaphoreInfoCount = u32(_swapchains.size()),
                .pSignalSemaphoreInfos = signalInfos,
            }), nullptr));
            rhi::stats::TimeAdaptingToPresent += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();

            binaryWaits = NOVA_ALLOC_STACK(VkSemaphore, _swapchains.size());
            for (u32 i = 0; i < _swapchains.size(); ++i) {
                auto swapchain = _swapchains[i];
                binaryWaits[i] = swapchain->semaphores[swapchain->semaphoreIndex];
                swapchain->semaphoreIndex = (swapchain->semaphoreIndex + 1) % swapchain->semaphores.size();
            }
        }

        auto vkSwapchains = NOVA_ALLOC_STACK(VkSwapchainKHR, _swapchains.size());
        auto indices = NOVA_ALLOC_STACK(u32, _swapchains.size());
        for (u32 i = 0; i < _swapchains.size(); ++i) {
            auto swapchain = _swapchains[i];
            vkSwapchains[i] = swapchain->swapchain;
            indices[i] = swapchain->index;
        }

        auto results = NOVA_ALLOC_STACK(VkResult, _swapchains.size());
        auto start = std::chrono::steady_clock::now();
        vkQueuePresentKHR(impl->handle, Temp(VkPresentInfoKHR {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = binaryWaits ? u32(_swapchains.size()) : 0u,
            .pWaitSemaphores = binaryWaits,
            .swapchainCount = u32(_swapchains.size()),
            .pSwapchains = vkSwapchains,
            .pImageIndices = indices,
            .pResults = results,
        }));
        rhi::stats::TimePresenting += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();

        for (u32 i = 0; i < _swapchains.size(); ++i) {
            if (results[i] == VK_ERROR_OUT_OF_DATE_KHR || results[i] == VK_SUBOPTIMAL_KHR) {
                NOVA_LOG("Swapchain[{}] present returned out-of-date/suboptimal ({})", (void*)_swapchains[i]->swapchain, int(results[i]));
                _swapchains[i]->invalid = true;
            } else {
                VkCall(results[i]);
            }
        }
    }

    void CommandList::Present(HSwapchain swapchain) const
    {
        Transition(swapchain().GetCurrent(),
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_2_NONE,
            0);
    }
}