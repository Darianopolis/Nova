#include "nova_VulkanDXGISwapchain.hpp"

#include <nova/window/nova_Window.hpp>

namespace nova
{
    struct DXGISwapchainStrategy : SwapchainStrategy
    {
        virtual void Destroy(Swapchain swapchain) final override
        {
            if (!swapchain) {
                return;
            }

            auto* impl = static_cast<DXGISwapchainData*>(swapchain.operator->());

            DestroySwapchainImages(swapchain);

            impl->dxswapchain->Release();

            if (impl->comp_enabled) {
                impl->dcomp_target->Release();
                impl->dcomp_visual->Release();
                impl->dcomp_device->Release();
            }

            impl->dxgi_device->Release();

            impl->dxqueue->Release();
            impl->dxdevice->Release();
            impl->dxadapter->Release();
            impl->dxfactory->Release();

            delete impl;
        }

        virtual Image Target(Swapchain swapchain) final override
        {
            auto* data = DXGISwapchainData::Get(swapchain);
            return data->images[data->index];
        }

        virtual Vec2U Extent(Swapchain swapchain) final override
        {
            auto* data = DXGISwapchainData::Get(swapchain);
            return { data->extent.width, data->extent.height };
        }

        virtual nova::Format Format(Swapchain swapchain) final override
        {
            return DXGISwapchainData::Get(swapchain)->format;
        }

        virtual SyncPoint Acquire(Queue queue, Span<HSwapchain> swapchains, bool* out_any_resized) final override
        {
            bool any_resized = false;

            for (auto _swapchain : swapchains) {
                auto* swapchain = DXGISwapchainData::Get(_swapchain);
                RECT rect;
                GetClientRect(swapchain->dxhwnd, &rect);
                if (rect.right > 0 && rect.bottom > 0 /* allow rendering to continue while minimized, add flag for this? */) {
                    vkh::Check(queue->context->vkQueueWaitIdle(queue->handle));
                    ResizeSwapchain(_swapchain, u32(rect.right), u32(rect.bottom));
                }

                swapchain->index = swapchain->dxswapchain->GetCurrentBackBufferIndex();
            }

            if (out_any_resized) {
                *out_any_resized = any_resized;
            }

            // TODO: Should handle empty fences being passed in?
            return queue->fence;
        }

        virtual void PreparePresent(CommandList cmd, HSwapchain swapchain) final override
        {
            cmd->Transition(Target(swapchain),
                VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
        }

        virtual void Present(Queue queue, Span<HSwapchain> swapchains, Span<SyncPoint> waits, PresentFlag flags) final override
        {
            NOVA_IGNORE(flags);

            NOVA_STACK_POINT();

            if (waits.size()) {
                auto semaphores = NOVA_STACK_ALLOC(VkSemaphore, waits.size());
                auto values = NOVA_STACK_ALLOC(u64, waits.size());
                for (u32 i = 0; i < waits.size(); ++i) {
                    semaphores[i] = waits[i].fence->semaphore;
                    values[i] = waits[i].Value();
                }

                vkh::Check(queue->context->vkWaitSemaphores(queue->context->device, PtrTo(VkSemaphoreWaitInfo {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                    .semaphoreCount = u32(waits.size()),
                    .pSemaphores = semaphores,
                    .pValues = values,
                }), UINT64_MAX));
            }

            for (auto _swapchain : swapchains) {
                auto* swapchain = DXGISwapchainData::Get(_swapchain);
                // TODO: Parameterize sync interval on present mode
                // TODO: Allow tearing
                swapchain->dxswapchain->Present1(0, 0, PtrTo(DXGI_PRESENT_PARAMETERS {}));
                // swapchain->dxswapchain->Present1(1, 0, PtrTo(DXGI_PRESENT_PARAMETERS {}));
                if (swapchain->comp_enabled) {
                    swapchain->dcomp_visual->SetContent(swapchain->dxswapchain);
                    swapchain->dcomp_target->SetRoot(swapchain->dcomp_visual);
                    swapchain->dcomp_device->Commit();
                }
            }
            for (auto _swapchain : swapchains) {
                auto* swapchain = DXGISwapchainData::Get(_swapchain);
                if (swapchain->comp_enabled) {
                    swapchain->dcomp_device->WaitForCommitCompletion();
                }
            }
        }

        void InitSwapchain(Swapchain _swapchain)
        {
            auto* swapchain = DXGISwapchainData::Get(_swapchain);
            auto context = swapchain->context;

            ::CreateDXGIFactory2(0, IID_PPV_ARGS(&swapchain->dxfactory));

            VkPhysicalDeviceIDProperties id_props { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
            context->vkGetPhysicalDeviceProperties2(context->gpu, PtrTo(VkPhysicalDeviceProperties2KHR {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                .pNext = &id_props,
            }));

            LUID* pluid = reinterpret_cast<LUID*>(&id_props.deviceLUID);

            swapchain->dxfactory->EnumAdapterByLuid(*pluid, IID_PPV_ARGS(&swapchain->dxadapter));

            ::D3D12CreateDevice(swapchain->dxadapter, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&swapchain->dxdevice));

            swapchain->dxdevice->CreateCommandQueue(PtrTo(D3D12_COMMAND_QUEUE_DESC {
                .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
                .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
                .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                .NodeMask = 1,
            }), IID_PPV_ARGS(&swapchain->dxqueue));

            {
                ID3D11Device* d11_device = nullptr;
                NOVA_DEFER(&) { if (d11_device) d11_device->Release(); };

                ::D3D11CreateDevice(nullptr,
                    D3D_DRIVER_TYPE_HARDWARE,
                    nullptr,
                    D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                    nullptr,
                    0,
                    D3D11_SDK_VERSION,
                    &d11_device,
                    nullptr,
                    nullptr);

                d11_device->QueryInterface(&swapchain->dxgi_device);
                if (!swapchain->dxgi_device) {
                    NOVA_THROW("DXGI Swapchain :: Failed to get DXGI device");
                }
            }

            if (swapchain->comp_enabled) {
                DCompositionCreateDevice(swapchain->dxgi_device, IID_PPV_ARGS(&swapchain->dcomp_device));

                swapchain->dcomp_device->CreateVisual(&swapchain->dcomp_visual);
            }

            swapchain->vkGetMemoryWin32HandlePropertiesKHR = (PFN_vkGetMemoryWin32HandlePropertiesKHR)
                swapchain->context->vkGetInstanceProcAddr(swapchain->context->instance, "vkGetMemoryWin32HandlePropertiesKHR");

            if (!swapchain->vkGetMemoryWin32HandlePropertiesKHR) {
                NOVA_THROW("DXGI Swapchain :: Failed to load vkGetMemoryWin32HandlePropertiesKHR");
            }
        }

        void CreateSwapchain(Swapchain _swapchain, HWND hwnd, u32 width, u32 height)
        {
            auto* swapchain = DXGISwapchainData::Get(_swapchain);

            if (swapchain->comp_enabled) {
                swapchain->dcomp_device->CreateTargetForHwnd(hwnd, false, &swapchain->dcomp_target);
            }

            swapchain->dxhwnd = hwnd;
            swapchain->extent = { width, height };

            // TODO: Parameterize!

            DXGI_SWAP_CHAIN_DESC1 desc {
                .Width = width,
                .Height = height,
                .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                .Stereo = FALSE,
                .SampleDesc = { 1, 0 },
                .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
                .BufferCount = swapchain->image_count,
                // .Scaling = DXGI_SCALING_STRETCH,
                .Scaling = DXGI_SCALING_NONE,
                .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
                .AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED,
                .Flags = 0,
            };

            // TODO: DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT

            IDXGISwapChain1* swapchain1 = nullptr;
            HRESULT res;
            if (swapchain->comp_enabled) {
                res = swapchain->dxfactory->CreateSwapChainForComposition(swapchain->dxqueue, &desc, nullptr, &swapchain1);
            } else {
                desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
                res = swapchain->dxfactory->CreateSwapChainForHwnd(swapchain->dxqueue, hwnd, &desc, nullptr, nullptr, &swapchain1);
            }
            if (FAILED(res)) {
                NOVA_THROW("Failed to create swapchain: {:#x}", (u32)res);
            }

            swapchain1->QueryInterface(&swapchain->dxswapchain);

            swapchain->dxfactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        }

        void ResizeSwapchain(Swapchain _swapchain, u32 width, u32 height)
        {
            auto* swapchain = DXGISwapchainData::Get(_swapchain);

            NOVA_STACK_POINT();

            if (swapchain->extent.width == width && swapchain->extent.height == height) {
                return;
            }

            DestroySwapchainImages(_swapchain);

            auto node_masks = NOVA_STACK_ALLOC(UINT, swapchain->image_count);
            auto queues = NOVA_STACK_ALLOC(IUnknown*, swapchain->image_count);
            for (u32 i = 0; i < swapchain->image_count; ++i) {
                node_masks[i] = 1;
                queues[i] = swapchain->dxqueue;
            }

            auto res = swapchain->dxswapchain->ResizeBuffers1(0, width, height, DXGI_FORMAT_UNKNOWN, 0, node_masks, queues);
            if (FAILED(res)) {
                NOVA_THROW("Error resizing swapchain: {0} ({0:#x})", u32(res));
            }

            swapchain->extent = { width, height };

            GetSwapchainImages(_swapchain);
        }

        void DestroySwapchainImages(Swapchain _swapchain)
        {
            auto* swapchain = DXGISwapchainData::Get(_swapchain);

            for (u32 i = 0; i < swapchain->image_count; ++i) {
                swapchain->context->vkFreeMemory(swapchain->context->device, swapchain->dxmemory[i], swapchain->context->alloc);
                ::CloseHandle(swapchain->dxhandles[i]);
                auto vkimage = swapchain->images[i]->image;
                swapchain->images[i].Destroy();
                swapchain->context->vkDestroyImage(swapchain->context->device, vkimage, swapchain->context->alloc);
            }
        }

        void GetSwapchainImages(Swapchain _swapchain)
        {
            auto* swapchain = DXGISwapchainData::Get(_swapchain);

            swapchain->images.resize(swapchain->image_count);
            swapchain->dxhandles.resize(swapchain->image_count);
            swapchain->dxmemory.resize(swapchain->image_count);

            for (u32 i = 0; i < swapchain->image_count; ++i) {
                ID3D12Resource* dx_image = nullptr;
                swapchain->dxswapchain->GetBuffer(i, IID_PPV_ARGS(&dx_image));
                NOVA_DEFER(&) { dx_image->Release(); };
                auto  dx_image_desc = dx_image->GetDesc();

                D3D12_HEAP_PROPERTIES dx_image_heap;
                D3D12_HEAP_FLAGS dx_image_heap_flags;
                dx_image->GetHeapProperties(&dx_image_heap, &dx_image_heap_flags);

                VkExternalMemoryImageCreateInfoKHR eii {
                    .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
                    .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
                };

                VkImageCreateInfo ii {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                    .pNext = &eii,
                    .flags = 0,
                    .imageType = VK_IMAGE_TYPE_2D,
                    .format = VK_FORMAT_R8G8B8A8_UNORM,
                    .extent = { u32(dx_image_desc.Width), u32(dx_image_desc.Height), 1 },
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .usage = GetVulkanImageUsage(swapchain->usage)
                        | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                };

                VkImage vkimage;
                vkh::Check(swapchain->context->vkCreateImage(swapchain->context->device, &ii, swapchain->context->alloc, &vkimage));

                HANDLE handle = nullptr;
                swapchain->dxdevice->CreateSharedHandle(dx_image, nullptr, GENERIC_ALL, nullptr, &handle);
                swapchain->dxhandles[i] = handle;

                VkMemoryWin32HandlePropertiesKHR win32_mem_props{
                    .sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
                    .memoryTypeBits = ~0u,
                };

                vkh::Check(swapchain->vkGetMemoryWin32HandlePropertiesKHR(swapchain->context->device,
                    VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
                    handle,
                    &win32_mem_props));

                VkMemoryRequirements mem_req;
                swapchain->context->vkGetImageMemoryRequirements(swapchain->context->device, vkimage, &mem_req);

                win32_mem_props.memoryTypeBits &= mem_req.memoryTypeBits;

                VkPhysicalDeviceMemoryProperties mem_props;
                swapchain->context->vkGetPhysicalDeviceMemoryProperties(swapchain->context->gpu, &mem_props);

                u32 mem_type_index = UINT_MAX;
                for (u32 im = 0; im < mem_props.memoryTypeCount; ++im) {
                    uint32_t current_bit = 1 << im;
                    if ((win32_mem_props.memoryTypeBits & current_bit) == current_bit) {
                        if (mem_props.memoryTypes[im].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                            mem_type_index = im;
                            break;
                        }
                    }
                }

                if (mem_type_index == UINT_MAX) {
                    NOVA_THROW("No valid memory type found!");
                }

                VkMemoryDedicatedAllocateInfoKHR dii {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
                    .image = vkimage,
                };

                VkImportMemoryWin32HandleInfoKHR imi {
                    .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
                    .pNext = &dii,
                    .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
                    .handle = handle,
                    .name = nullptr,
                };

                VkDeviceMemory memory;

                vkh::Check(swapchain->context->vkAllocateMemory(swapchain->context->device, PtrTo(VkMemoryAllocateInfo {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                    .pNext = &imi,
                    .allocationSize = mem_req.size,
                    .memoryTypeIndex = mem_type_index,
                }), swapchain->context->alloc, &memory));

                swapchain->dxmemory[i] = memory;

                vkh::Check(swapchain->context->vkBindImageMemory(swapchain->context->device, vkimage, memory, 0));

                {
                    auto& image = (swapchain->images[i] = { new Image::Impl });
                    image->context = swapchain->context;

                    image->usage = swapchain->usage;
                    image->format = swapchain->format;
                    image->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                    image->extent = Vec3(swapchain->extent.width, swapchain->extent.height, 1);
                    image->mips = 1;
                    image->layers = 1;

                    image->image = vkimage;
                    vkh::Check(swapchain->context->vkCreateImageView(swapchain->context->device, PtrTo(VkImageViewCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                        .image = vkimage,
                        .viewType = VK_IMAGE_VIEW_TYPE_2D,
                        .format = GetVulkanFormat(swapchain->format).vk_format,
                        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
                    }), swapchain->context->alloc, &image->view));
                }
            }
        }
    };

    DXGISwapchainStrategy* GetDXGISwapchainStrategy()
    {
        static DXGISwapchainStrategy strategy;
        return &strategy;
    }

    Swapchain DXGISwapchain_Create(HContext context, Window window, ImageUsage usage, PresentMode present_mode)
    {
        auto strategy = GetDXGISwapchainStrategy();
        auto impl = new DXGISwapchainData;
        impl->strategy = strategy;
        impl->context = context;
        impl->usage = usage;
        impl->present_mode = present_mode;
        impl->image_count = 3;
        impl->format = Format::RGBA8_UNorm;

        // TODO: Supported formats (RGBA/BGRA)
        // TODO: HDR?

        HWND hwnd = (HWND)window.NativeHandle();

        RECT rect;
        GetClientRect(hwnd, &rect);

        Swapchain swapchain = { impl };

        strategy->InitSwapchain(swapchain);
        strategy->CreateSwapchain(swapchain, hwnd, u32(rect.right), u32(rect.bottom));
        strategy->GetSwapchainImages(swapchain);

        return swapchain;
    }
}