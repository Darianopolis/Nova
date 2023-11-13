#include "nova_VulkanRHI.hpp"

#include <nova/core/nova_Stack.hpp>

namespace nova
{
    Swapchain Swapchain::Create(HContext context, void* window, TextureUsage usage, PresentMode present_mode)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->usage = usage;
        impl->present_mode = present_mode;

        vkh::Check(impl->context->vkCreateWin32SurfaceKHR(context->instance, Temp(VkWin32SurfaceCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = GetModuleHandle(nullptr),
            .hwnd = HWND(window),
        }), context->alloc, &impl->surface));

        std::vector<VkSurfaceFormatKHR> surface_formats;
        vkh::Enumerate(surface_formats, impl->context->vkGetPhysicalDeviceSurfaceFormatsKHR, context->gpu, impl->surface);

        for (auto& surface_format : surface_formats) {
            if ((surface_format.format == VK_FORMAT_B8G8R8A8_UNORM
                    || surface_format.format == VK_FORMAT_R8G8B8A8_UNORM)) {
                impl->format = surface_format;
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
            impl->context->vkDestroySemaphore(impl->context->device, semaphore, impl->context->alloc);
        }

        for (auto& texture : impl->textures) {
            texture.Destroy();
        }

        if (impl->swapchain) {
            impl->context->vkDestroySwapchainKHR(impl->context->device, impl->swapchain, impl->context->alloc);
        }

        impl->context->vkDestroySurfaceKHR(impl->context->instance, impl->surface, impl->context->alloc);

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
        bool any_resized = false;

        for (auto swapchain : _swapchains) {
            do {
                VkSurfaceCapabilitiesKHR caps;
                vkh::Check(impl->context->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(impl->context->gpu, swapchain->surface, &caps));

                bool recreate = swapchain->invalid
                    || !swapchain->swapchain
                    || caps.currentExtent.width != swapchain->extent.width
                    || caps.currentExtent.height != swapchain->extent.height;

                if (recreate) {
                    any_resized |= recreate;

                    vkh::Check(impl->context->vkQueueWaitIdle(impl->handle));

                    auto old_swapchain = swapchain->swapchain;

                    auto vk_usage = GetVulkanImageUsage(swapchain->usage)
                        | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

                    swapchain->extent = caps.currentExtent;
                    vkh::Check(impl->context->vkCreateSwapchainKHR(impl->context->device, Temp(VkSwapchainCreateInfoKHR {
                        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                        .surface = swapchain->surface,
                        .minImageCount = caps.minImageCount,
                        .imageFormat = swapchain->format.format,
                        .imageColorSpace = swapchain->format.colorSpace,
                        .imageExtent = swapchain->extent,
                        .imageArrayLayers = 1,
                        .imageUsage = vk_usage,
                        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, // TODO: concurrent for async present
                        .queueFamilyIndexCount = 1,
                        .pQueueFamilyIndices = &impl->family,
                        .preTransform = caps.currentTransform,
                        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                        .presentMode = GetVulkanPresentMode(swapchain->present_mode),
                        .clipped = VK_TRUE,
                        .oldSwapchain = old_swapchain,
                    }), impl->context->alloc, &swapchain->swapchain));

                    swapchain->semaphore_index = false;

                    impl->context->vkDestroySwapchainKHR(impl->context->device, old_swapchain, impl->context->alloc);

                    std::vector<VkImage> vk_images;
                    vkh::Enumerate(vk_images, impl->context->vkGetSwapchainImagesKHR, impl->context->device, swapchain->swapchain);

                    while (swapchain->semaphores.size() < vk_images.size() * 2) {
                        auto& semaphore = swapchain->semaphores.emplace_back();
                        vkh::Check(impl->context->vkCreateSemaphore(impl->context->device, Temp(VkSemaphoreCreateInfo {
                            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                        }), impl->context->alloc, &semaphore));
                    }

                    for (auto& texture : swapchain->textures) {
                        texture.Destroy();
                    }
                    swapchain->textures.resize(vk_images.size());
                    for (uint32_t i = 0; i < vk_images.size(); ++i) {
                        auto& texture = (swapchain->textures[i] = { new Texture::Impl });
                        texture->context = impl->context;

                        texture->usage = swapchain->usage;
                        texture->format = swapchain.Unwrap().GetFormat();
                        texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                        texture->extent = Vec3(swapchain->extent.width, swapchain->extent.height, 1);
                        texture->mips = 1;
                        texture->layers = 1;

                        texture->image = vk_images[i];
                        vkh::Check(impl->context->vkCreateImageView(impl->context->device, Temp(VkImageViewCreateInfo {
                            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                            .image = vk_images[i],
                            .viewType = VK_IMAGE_VIEW_TYPE_2D,
                            .format = swapchain->format.format,
                            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
                        }), impl->context->alloc, &texture->view));
                    }
                }

                auto result = impl->context->vkAcquireNextImage2KHR(impl->context->device, Temp(VkAcquireNextImageInfoKHR {
                    .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
                    .swapchain = swapchain->swapchain,
                    .timeout = UINT64_MAX,
                    .semaphore = signals.size() ? swapchain->semaphores[swapchain->semaphore_index] : nullptr,
                    .deviceMask = 1,
                }), &swapchain->index);

                if (result != VK_SUCCESS) {
                    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
                        NOVA_LOG("Swapchain[{}] acquire returned out-of-date/suboptimal ({})", (void*)swapchain->swapchain, int(result));
                        swapchain->invalid = true;
                    } else {
                        vkh::Check(result);
                    }
                }
            } while (swapchain->invalid);
        }

        if (signals.size())
        {
            auto wait_infos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, _swapchains.size());
            for (u32 i = 0; i < _swapchains.size(); ++i) {
                auto swapchain = _swapchains[i];
                wait_infos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = swapchain->semaphores[swapchain->semaphore_index],
                };
                swapchain->semaphore_index = (swapchain->semaphore_index + 1) % swapchain->semaphores.size();
            }

            auto signal_infos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, signals.size());
            for (u32 i = 0; i < signals.size(); ++i) {
                auto signal = signals[i];
                signal_infos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = signal->semaphore,
                    .value = signals[i].Unwrap().Advance(),
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                };
            }

            auto start = std::chrono::steady_clock::now();
            vkh::Check(impl->context->vkQueueSubmit2(impl->handle, 1, Temp(VkSubmitInfo2 {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = u32(_swapchains.size()),
                .pWaitSemaphoreInfos = wait_infos,
                .signalSemaphoreInfoCount = u32(signals.size()),
                .pSignalSemaphoreInfos = signal_infos,
            }), nullptr));
            rhi::stats::TimeAdaptingFromAcquire += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        }

        return any_resized;
    }

    void Queue::Present(Span<HSwapchain> _swapchains, Span<HFence> waits, bool host_wait) const
    {
        VkSemaphore* binary_waits = nullptr;

        if (host_wait && waits.size()) {
            auto semaphores = NOVA_ALLOC_STACK(VkSemaphore, waits.size());
            auto values = NOVA_ALLOC_STACK(u64, waits.size());
            for (u32 i = 0; i < waits.size(); ++i) {
                semaphores[i] = waits[i]->semaphore;
                values[i] = waits[i]->value;
            }

            vkh::Check(impl->context->vkWaitSemaphores(impl->context->device, Temp(VkSemaphoreWaitInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                .semaphoreCount = u32(waits.size()),
                .pSemaphores = semaphores,
                .pValues = values,
            }), UINT64_MAX));

        } else if (waits.size()) {
            auto wait_infos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, waits.size());
            for (u32 i = 0; i < waits.size(); ++i) {
                wait_infos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = waits[i]->semaphore,
                    .value = waits[i]->value,
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                };
            }

            auto signal_infos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, _swapchains.size());
            for (u32 i = 0; i < _swapchains.size(); ++i) {
                auto swapchain = _swapchains[i];
                signal_infos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = swapchain->semaphores[swapchain->semaphore_index],
                };
            }

            auto start = std::chrono::steady_clock::now();
            vkh::Check(impl->context->vkQueueSubmit2(impl->handle, 1, Temp(VkSubmitInfo2 {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = u32(waits.size()),
                .pWaitSemaphoreInfos = wait_infos,
                .signalSemaphoreInfoCount = u32(_swapchains.size()),
                .pSignalSemaphoreInfos = signal_infos,
            }), nullptr));
            rhi::stats::TimeAdaptingToPresent += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();

            binary_waits = NOVA_ALLOC_STACK(VkSemaphore, _swapchains.size());
            for (u32 i = 0; i < _swapchains.size(); ++i) {
                auto swapchain = _swapchains[i];
                binary_waits[i] = swapchain->semaphores[swapchain->semaphore_index];
                swapchain->semaphore_index = (swapchain->semaphore_index + 1) % swapchain->semaphores.size();
            }
        }

        auto vk_swapchains = NOVA_ALLOC_STACK(VkSwapchainKHR, _swapchains.size());
        auto indices = NOVA_ALLOC_STACK(u32, _swapchains.size());
        for (u32 i = 0; i < _swapchains.size(); ++i) {
            auto swapchain = _swapchains[i];
            vk_swapchains[i] = swapchain->swapchain;
            indices[i] = swapchain->index;
        }

        auto results = NOVA_ALLOC_STACK(VkResult, _swapchains.size());
        auto start = std::chrono::steady_clock::now();
        impl->context->vkQueuePresentKHR(impl->handle, Temp(VkPresentInfoKHR {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = binary_waits ? u32(_swapchains.size()) : 0u,
            .pWaitSemaphores = binary_waits,
            .swapchainCount = u32(_swapchains.size()),
            .pSwapchains = vk_swapchains,
            .pImageIndices = indices,
            .pResults = results,
        }));
        rhi::stats::TimePresenting += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();

        for (u32 i = 0; i < _swapchains.size(); ++i) {
            if (results[i] == VK_ERROR_OUT_OF_DATE_KHR || results[i] == VK_SUBOPTIMAL_KHR) {
                if (impl->context->config.trace) {
                    NOVA_LOG("Swapchain[{}] present returned out-of-date/suboptimal ({})", (void*)_swapchains[i]->swapchain, int(results[i]));
                }
                _swapchains[i]->invalid = true;
            } else {
                vkh::Check(results[i]);
            }
        }
    }

    void CommandList::Present(HSwapchain swapchain) const
    {
        impl->Transition(swapchain.Unwrap().GetCurrent(),
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_2_NONE);
    }
}