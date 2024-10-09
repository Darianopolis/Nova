#include "VulkanKHRSwapchain.hpp"

#include <nova/window/Window.hpp>

namespace nova
{
    struct KHRSwapchainStrategy : SwapchainStrategy
    {
        virtual void Destroy(Swapchain swapchain) final override
        {
            if (!swapchain) {
                return;
            }

            auto* impl = static_cast<KHRSwapchainData*>(swapchain.operator->());

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
        }

        virtual Image Target(Swapchain swapchain) final override
        {
            auto* data = KHRSwapchainData::Get(swapchain);
            return data->images[data->index];
        }

        virtual Vec2U Extent(Swapchain swapchain) final override
        {
            auto* data = KHRSwapchainData::Get(swapchain);
            return { data->extent.width, data->extent.height };
        }

        virtual nova::Format Format(Swapchain swapchain) final override
        {
            return FromVulkanFormat(KHRSwapchainData::Get(swapchain)->format.format);
        }

        static
        bool EnsureSwapchain(KHRSwapchainStrategy* strategy, Queue queue, Swapchain _swapchain)
        {
            bool resized = false;

            auto* swapchain = KHRSwapchainData::Get(_swapchain);
            do {
                VkSurfaceCapabilitiesKHR caps;
                vkh::Check(queue->context->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(queue->context->gpu, swapchain->surface, &caps));

                bool recreate = swapchain->invalid
                    || !swapchain->swapchain;
                    // || caps.currentExtent.width != swapchain->extent.width
                    // || caps.currentExtent.height != swapchain->extent.height;

                if (caps.currentExtent.width == 0 && caps.currentExtent.height == 0) {
                    swapchain->invalid = false;
                    break;
                }

                if (recreate) {
                    resized |= recreate;

                    vkh::Check(queue->context->vkQueueWaitIdle(queue->handle));

                    auto old_swapchain = swapchain->swapchain;

                    auto vk_usage = GetVulkanImageUsage(swapchain->usage)
                        | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

                    swapchain->extent = caps.currentExtent;
                    vkh::Check(queue->context->vkCreateSwapchainKHR(queue->context->device, PtrTo(VkSwapchainCreateInfoKHR {
                        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                        .surface = swapchain->surface,
                        .minImageCount = caps.minImageCount + 1,
                        .imageFormat = swapchain->format.format,
                        .imageColorSpace = swapchain->format.colorSpace,
                        .imageExtent = swapchain->extent,
                        .imageArrayLayers = 1,
                        .imageUsage = vk_usage,
                        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                        .queueFamilyIndexCount = 1,
                        .pQueueFamilyIndices = &queue->family,
                        .preTransform = caps.currentTransform,
                        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                        .presentMode = GetVulkanPresentMode(swapchain->present_mode),
                        .clipped = VK_TRUE,
                        .oldSwapchain = old_swapchain,
                    }), queue->context->alloc, &swapchain->swapchain));

                    swapchain->semaphore_index = false;

                    queue->context->vkDestroySwapchainKHR(queue->context->device, old_swapchain, queue->context->alloc);

                    NOVA_STACK_POINT();

                    auto vk_images = NOVA_STACK_VKH_ENUMERATE(VkImage,
                        queue->context->vkGetSwapchainImagesKHR, queue->context->device, swapchain->swapchain);

                    while (swapchain->semaphores.size() < vk_images.size() * 2) {
                        auto& semaphore = swapchain->semaphores.emplace_back();
                        vkh::Check(queue->context->vkCreateSemaphore(queue->context->device, PtrTo(VkSemaphoreCreateInfo {
                            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                        }), queue->context->alloc, &semaphore));
                    }

                    for (auto& image : swapchain->images) {
                        image.Destroy();
                    }
                    swapchain->images.resize(vk_images.size());
                    for (uint32_t i = 0; i < vk_images.size(); ++i) {
                        auto& image = (swapchain->images[i] = { new Image::Impl });
                        image->context = queue->context;

                        image->usage = swapchain->usage;
                        image->format = strategy->Format(_swapchain);
                        image->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                        image->extent = Vec3(swapchain->extent.width, swapchain->extent.height, 1);
                        image->mips = 1;
                        image->layers = 1;

                        image->image = vk_images[i];
                        vkh::Check(queue->context->vkCreateImageView(queue->context->device, PtrTo(VkImageViewCreateInfo {
                            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                            .image = vk_images[i],
                            .viewType = VK_IMAGE_VIEW_TYPE_2D,
                            .format = swapchain->format.format,
                            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
                        }), queue->context->alloc, &image->view));
                    }
                }

                auto result = queue->context->vkAcquireNextImage2KHR(queue->context->device, PtrTo(VkAcquireNextImageInfoKHR {
                    .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
                    .swapchain = swapchain->swapchain,
                    .timeout = UINT64_MAX,
                    .semaphore = swapchain->semaphores[swapchain->semaphore_index],
                    .deviceMask = 1,
                }), &swapchain->index);

                if (result != VK_SUCCESS) {
                    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
                        // Log("Swapchain[{}] acquire returned {} ", (void*)swapchain->swapchain, result == VK_ERROR_OUT_OF_DATE_KHR ? "out-of-date" : "suboptimal");
                        swapchain->invalid = true;
                    } else {
                        vkh::Check(result);
                    }
                } else {
                    swapchain->invalid = false;
                }
            } while (swapchain->invalid);

            return resized;
        }

        virtual SyncPoint Acquire(Queue queue, Span<HSwapchain> swapchains, bool* out_any_resized) final override
        {
            bool any_resized = false;

            for (auto swapchain : swapchains) {
                any_resized |= EnsureSwapchain(this, queue, swapchain);
            }

            {
                NOVA_STACK_POINT();

                auto wait_infos = NOVA_STACK_ALLOC(VkSemaphoreSubmitInfo, swapchains.size());
                for (u32 i = 0; i < swapchains.size(); ++i) {
                    auto swapchain = KHRSwapchainData::Get(swapchains[i]);
                    wait_infos[i] = {
                        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                        .semaphore = swapchain->semaphores[swapchain->semaphore_index],
                    };
                    swapchain->semaphore_index = (swapchain->semaphore_index + 1) % swapchain->semaphores.size();
                }

                VkSemaphoreSubmitInfo signal_info {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = queue->fence->semaphore,
                    .value = queue->fence.Advance(),
                    // TODO: Additional granularity?
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                };

                auto start = std::chrono::steady_clock::now();
                vkh::Check(queue->context->vkQueueSubmit2(queue->handle, 1, PtrTo(VkSubmitInfo2 {
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                    .waitSemaphoreInfoCount = u32(swapchains.size()),
                    .pWaitSemaphoreInfos = wait_infos,
                    .signalSemaphoreInfoCount = 1,
                    .pSignalSemaphoreInfos = &signal_info,
                }), nullptr));
                rhi::stats::TimeAdaptingFromAcquire += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
            }

            if (out_any_resized) {
                *out_any_resized = any_resized;
            }

            return queue->fence;
        }

        virtual void PreparePresent(CommandList cmd, HSwapchain swapchain) final override
        {
            cmd->Transition(swapchain.Unwrap().Target(),
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_2_NONE);
        }

        virtual void Present(Queue queue, Span<HSwapchain> swapchains, Span<SyncPoint> waits, PresentFlag flags) final override
        {
            if (swapchains.empty()) return;
            NOVA_STACK_POINT();

            VkSemaphore* binary_waits = nullptr;

            if ((flags >= PresentFlag::HostWaitOnFences) && waits.size()) {
                auto semaphores = NOVA_STACK_ALLOC(VkSemaphore, waits.size());
                auto values = NOVA_STACK_ALLOC(u64, waits.size());
                for (u32 i = 0; i < waits.size(); ++i) {
                    semaphores[i] = waits[i].fence->semaphore;
                    values[i] = waits[i].value;
                }

                vkh::Check(queue->context->vkWaitSemaphores(queue->context->device, PtrTo(VkSemaphoreWaitInfo {
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
                        .semaphore = waits[i].fence->semaphore,
                        .value = waits[i].Value(),
                    };
                }

                auto signal_infos = NOVA_STACK_ALLOC(VkSemaphoreSubmitInfo, swapchains.size());
                for (u32 i = 0; i < swapchains.size(); ++i) {
                    auto swapchain = KHRSwapchainData::Get(swapchains[i]);
                    signal_infos[i] = {
                        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                        .semaphore = swapchain->semaphores[swapchain->semaphore_index],
                    };
                }

                auto start = std::chrono::steady_clock::now();
                vkh::Check(queue->context->vkQueueSubmit2(queue->handle, 1, PtrTo(VkSubmitInfo2 {
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                    .waitSemaphoreInfoCount = u32(waits.size()),
                    .pWaitSemaphoreInfos = wait_infos,
                    .signalSemaphoreInfoCount = u32(swapchains.size()),
                    .pSignalSemaphoreInfos = signal_infos,
                }), nullptr));
                rhi::stats::TimeAdaptingToPresent += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();

                binary_waits = NOVA_STACK_ALLOC(VkSemaphore, swapchains.size());
                for (u32 i = 0; i < swapchains.size(); ++i) {
                    auto swapchain = KHRSwapchainData::Get(swapchains[i]);
                    binary_waits[i] = swapchain->semaphores[swapchain->semaphore_index];
                    swapchain->semaphore_index = (swapchain->semaphore_index + 1) % swapchain->semaphores.size();
                }
            }

            auto vk_swapchains = NOVA_STACK_ALLOC(VkSwapchainKHR, swapchains.size());
            auto indices = NOVA_STACK_ALLOC(u32, swapchains.size());
            for (u32 i = 0; i < swapchains.size(); ++i) {
                auto swapchain = KHRSwapchainData::Get(swapchains[i]);
                vk_swapchains[i] = swapchain->swapchain;
                indices[i] = swapchain->index;
            }

            auto results = NOVA_STACK_ALLOC(VkResult, swapchains.size());
            auto start = std::chrono::steady_clock::now();
            queue->context->vkQueuePresentKHR(queue->handle, PtrTo(VkPresentInfoKHR {
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
                    auto swapchain = KHRSwapchainData::Get(swapchains[i]);
                    // Log("Swapchain[{}] present returned {} ", (void*)swapchain->swapchain, results[i] == VK_ERROR_OUT_OF_DATE_KHR ? "out-of-date" : "suboptimal");
                    swapchain->invalid = true;
                } else {
                    vkh::Check(results[i]);
                }
            }
        }
    };

    KHRSwapchainStrategy* GetKHRSwapchainStrategy()
    {
        static KHRSwapchainStrategy strategy;
        return &strategy;
    }

    Swapchain KHRSwapchain_Create(HContext context, Window window, ImageUsage usage, PresentMode present_mode)
    {
        auto impl = new KHRSwapchainData;
        impl->strategy = GetKHRSwapchainStrategy();
        impl->context = context;
        impl->usage = usage;
        impl->present_mode = present_mode;

        impl->surface = Platform_CreateVulkanSurface(context, window.NativeHandle());

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
}
