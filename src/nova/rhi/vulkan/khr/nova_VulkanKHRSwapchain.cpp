#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

#include <nova/core/nova_Stack.hpp>

namespace nova
{
    template<>
    struct Handle<Swapchain>::Impl
    {
        Context context = {};

        VkSurfaceKHR      surface = {};
        VkSwapchainKHR  swapchain = {};
        VkSurfaceFormatKHR format = { VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        ImageUsage          usage = {};
        PresentMode  present_mode = PresentMode::Fifo;
        std::vector<Image> images = {};
        uint32_t            index = UINT32_MAX;
        VkExtent2D         extent = { 0, 0 };
        bool              invalid = false;

        std::vector<VkSemaphore> semaphores = {};
        u32                 semaphore_index = 0;
    };

    Swapchain Swapchain::Create(HContext context, void* window, ImageUsage usage, PresentMode present_mode)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->usage = usage;
        impl->present_mode = present_mode;

        impl->surface = Platform_CreateVulkanSurface(context, window);

        NOVA_STACK_POINT();

        auto surface_formats = NOVA_STACK_VKH_ENUMERATE(VkSurfaceFormatKHR,
            impl->context->vkGetPhysicalDeviceSurfaceFormatsKHR, context->gpu, impl->surface);

        for (auto& surface_format : surface_formats) {
            if ((surface_format.format == VK_FORMAT_B8G8R8A8_UNORM
                    || surface_format.format == VK_FORMAT_R8G8B8A8_UNORM)) {
            // if ((surface_format.format == VK_FORMAT_B8G8R8A8_SRGB
            //     || surface_format.format == VK_FORMAT_R8G8B8A8_SRGB)) {
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

        for (auto& image : impl->images) {
            image.Destroy();
        }

        if (impl->swapchain) {
            impl->context->vkDestroySwapchainKHR(impl->context->device, impl->swapchain, impl->context->alloc);
        }

        impl->context->vkDestroySurfaceKHR(impl->context->instance, impl->surface, impl->context->alloc);

        delete impl;
        impl = nullptr;
    }

    Image Swapchain::Target() const
    {
        return impl->images[impl->index];
    }

    Vec2U Swapchain::Extent() const
    {
        return { impl->extent.width, impl->extent.height };
    }

    Format Swapchain::Format() const
    {
        return FromVulkanFormat(impl->format.format);
    }

// -----------------------------------------------------------------------------

    bool Queue::Acquire(Span<HSwapchain> swapchains, Span<HFence> signals) const
    {
        bool any_resized = false;

        for (auto swapchain : swapchains) {
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

                    NOVA_STACK_POINT();

                    auto vk_images = NOVA_STACK_VKH_ENUMERATE(VkImage,
                        impl->context->vkGetSwapchainImagesKHR, impl->context->device, swapchain->swapchain);

                    while (swapchain->semaphores.size() < vk_images.size() * 2) {
                        auto& semaphore = swapchain->semaphores.emplace_back();
                        vkh::Check(impl->context->vkCreateSemaphore(impl->context->device, Temp(VkSemaphoreCreateInfo {
                            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                        }), impl->context->alloc, &semaphore));
                    }

                    for (auto& image : swapchain->images) {
                        image.Destroy();
                    }
                    swapchain->images.resize(vk_images.size());
                    for (uint32_t i = 0; i < vk_images.size(); ++i) {
                        auto& image = (swapchain->images[i] = { new Image::Impl });
                        image->context = impl->context;

                        image->usage = swapchain->usage;
                        image->format = swapchain.Unwrap().Format();
                        image->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                        image->extent = Vec3(swapchain->extent.width, swapchain->extent.height, 1);
                        image->mips = 1;
                        image->layers = 1;

                        image->image = vk_images[i];
                        vkh::Check(impl->context->vkCreateImageView(impl->context->device, Temp(VkImageViewCreateInfo {
                            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                            .image = vk_images[i],
                            .viewType = VK_IMAGE_VIEW_TYPE_2D,
                            .format = swapchain->format.format,
                            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
                        }), impl->context->alloc, &image->view));
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
            NOVA_STACK_POINT();

            auto wait_infos = NOVA_STACK_ALLOC(VkSemaphoreSubmitInfo, swapchains.size());
            for (u32 i = 0; i < swapchains.size(); ++i) {
                auto swapchain = swapchains[i];
                wait_infos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = swapchain->semaphores[swapchain->semaphore_index],
                };
                swapchain->semaphore_index = (swapchain->semaphore_index + 1) % swapchain->semaphores.size();
            }

            auto signal_infos = NOVA_STACK_ALLOC(VkSemaphoreSubmitInfo, signals.size());
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
                .waitSemaphoreInfoCount = u32(swapchains.size()),
                .pWaitSemaphoreInfos = wait_infos,
                .signalSemaphoreInfoCount = u32(signals.size()),
                .pSignalSemaphoreInfos = signal_infos,
            }), nullptr));
            rhi::stats::TimeAdaptingFromAcquire += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        }

        return any_resized;
    }

    void Queue::Present(Span<HSwapchain> swapchains, Span<HFence> waits, PresentFlag flags) const
    {
        NOVA_STACK_POINT();

        VkSemaphore* binary_waits = nullptr;

        if ((flags >= PresentFlag::HostWaitOnFences) && waits.size()) {
            auto semaphores = NOVA_STACK_ALLOC(VkSemaphore, waits.size());
            auto values = NOVA_STACK_ALLOC(u64, waits.size());
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
            auto wait_infos = NOVA_STACK_ALLOC(VkSemaphoreSubmitInfo, waits.size());
            for (u32 i = 0; i < waits.size(); ++i) {
                wait_infos[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = waits[i]->semaphore,
                    .value = waits[i]->value,
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                };
            }

            auto signal_infos = NOVA_STACK_ALLOC(VkSemaphoreSubmitInfo, swapchains.size());
            for (u32 i = 0; i < swapchains.size(); ++i) {
                auto swapchain = swapchains[i];
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
                .signalSemaphoreInfoCount = u32(swapchains.size()),
                .pSignalSemaphoreInfos = signal_infos,
            }), nullptr));
            rhi::stats::TimeAdaptingToPresent += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();

            binary_waits = NOVA_STACK_ALLOC(VkSemaphore, swapchains.size());
            for (u32 i = 0; i < swapchains.size(); ++i) {
                auto swapchain = swapchains[i];
                binary_waits[i] = swapchain->semaphores[swapchain->semaphore_index];
                swapchain->semaphore_index = (swapchain->semaphore_index + 1) % swapchain->semaphores.size();
            }
        }

        auto vk_swapchains = NOVA_STACK_ALLOC(VkSwapchainKHR, swapchains.size());
        auto indices = NOVA_STACK_ALLOC(u32, swapchains.size());
        for (u32 i = 0; i < swapchains.size(); ++i) {
            auto swapchain = swapchains[i];
            vk_swapchains[i] = swapchain->swapchain;
            indices[i] = swapchain->index;
        }

        auto results = NOVA_STACK_ALLOC(VkResult, swapchains.size());
        auto start = std::chrono::steady_clock::now();
        impl->context->vkQueuePresentKHR(impl->handle, Temp(VkPresentInfoKHR {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = binary_waits ? u32(swapchains.size()) : 0u,
            .pWaitSemaphores = binary_waits,
            .swapchainCount = u32(swapchains.size()),
            .pSwapchains = vk_swapchains,
            .pImageIndices = indices,
            .pResults = results,
        }));
        rhi::stats::TimePresenting += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();

        for (u32 i = 0; i < swapchains.size(); ++i) {
            if (results[i] == VK_ERROR_OUT_OF_DATE_KHR || results[i] == VK_SUBOPTIMAL_KHR) {
                if (impl->context->config.trace) {
                    NOVA_LOG("Swapchain[{}] present returned out-of-date/suboptimal ({})", (void*)swapchains[i]->swapchain, int(results[i]));
                }
                swapchains[i]->invalid = true;
            } else {
                vkh::Check(results[i]);
            }
        }
    }

    void CommandList::Present(HSwapchain swapchain) const
    {
        impl->Transition(swapchain.Unwrap().Target(),
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_2_NONE);
    }
}