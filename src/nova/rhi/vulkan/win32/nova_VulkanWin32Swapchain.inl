#include <nova/core/win32/nova_Win32Include.hpp>

#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <nova/core/nova_Guards.hpp>

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

#include <nova/core/win32/nova_Win32Include.hpp>
#include <d3d11_1.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dcomp.h>

#include <nova/core/nova_Stack.hpp>

namespace nova
{
    template<>
    struct Handle<Swapchain>::Impl
    {
        Context context = {};

        // TODO: This DX state should be centralized >:(
        IDXGIFactory7*    dxfactory = nullptr;
        IDXGIAdapter4*    dxadapter = nullptr;
        ID3D12Device5*     dxdevice = nullptr;
        ID3D12CommandQueue* dxqueue = nullptr;
        IDXGIDevice*    dxgi_device = nullptr;

        bool                 comp_enabled = true;
        IDCompositionDevice* dcomp_device = nullptr;
        IDCompositionVisual* dcomp_visual = nullptr;
        IDCompositionTarget* dcomp_target = nullptr;

        HWND dxhwnd = nullptr;
        IDXGISwapChain4*         dxswapchain = nullptr;
        std::vector<VkDeviceMemory> dxmemory;
        std::vector<HANDLE>        dxhandles;

        PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR = nullptr;

        u32           image_count = 0;
        Format             format = {};
        ImageUsage          usage = {};
        PresentMode  present_mode = PresentMode::Fifo;
        std::vector<Image> images = {};
        uint32_t            index = UINT32_MAX;
        VkExtent2D         extent = { 0, 0 };
    };

    void Win32_Init(Swapchain swapchain);
    void Win32_CreateSwapchain(Swapchain swapchain, HWND hwnd, u32 width, u32 height);
    void Win32_DestroySwapchainImages(Swapchain swapchain);
    void Win32_GetSwapchainImages(Swapchain swapchain);

    Swapchain Swapchain::Create(HContext context, void* window, ImageUsage usage, PresentMode present_mode)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->usage = usage;
        impl->present_mode = present_mode;
        impl->image_count = 3;
        impl->format = Format::RGBA8_UNorm;

        // TODO: Supported formats (RGBA/BGRA)
        // TODO: HDR?

        RECT rect;
        GetClientRect((HWND)window, &rect);

        Swapchain swapchain = { impl };

        Win32_Init(swapchain);
        Win32_CreateSwapchain(swapchain, (HWND)window, u32(rect.right), u32(rect.bottom));
        Win32_GetSwapchainImages(swapchain);

        return swapchain;
    }

    void Swapchain::Destroy()
    {
        if (!impl) {
            return;
        }

        Win32_DestroySwapchainImages(*this);

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
        impl = nullptr;
    }

    Image Swapchain::GetCurrent() const
    {
        return impl->images[impl->index];
    }

    Vec2U Swapchain::GetExtent() const
    {
        return { impl->extent.width, impl->extent.height };
    }

    Format Swapchain::GetFormat() const
    {
        return impl->format;
    }

// -----------------------------------------------------------------------------

    void Win32_Init(Swapchain swapchain)
    {
        auto context = swapchain->context;

        ::CreateDXGIFactory2(0, IID_PPV_ARGS(&swapchain->dxfactory));

        VkPhysicalDeviceIDProperties id_props { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
        context->vkGetPhysicalDeviceProperties2(context->gpu, Temp(VkPhysicalDeviceProperties2KHR {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &id_props,
        }));

        LUID* pluid = reinterpret_cast<LUID*>(&id_props.deviceLUID);

        swapchain->dxfactory->EnumAdapterByLuid(*pluid, IID_PPV_ARGS(&swapchain->dxadapter));

        ::D3D12CreateDevice(swapchain->dxadapter, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&swapchain->dxdevice));

        swapchain->dxdevice->CreateCommandQueue(Temp(D3D12_COMMAND_QUEUE_DESC {
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

    void Win32_CreateSwapchain(Swapchain swapchain, HWND hwnd, u32 width, u32 height)
    {
        if (swapchain->comp_enabled) {
            swapchain->dcomp_device->CreateTargetForHwnd(hwnd, false, &swapchain->dcomp_target);
        }

        swapchain->dxhwnd = hwnd;
        swapchain->extent = { width, height };

        DXGI_SWAP_CHAIN_DESC1 desc {
            .Width = width,
            .Height = height,
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .Stereo = FALSE,
            .SampleDesc = { 1, 0 },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = swapchain->image_count,
            .Scaling = DXGI_SCALING_STRETCH,
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

    void Win32_ResizeSwapchain(Swapchain swapchain, u32 width, u32 height)
    {
        if (swapchain->extent.width == width && swapchain->extent.height == height) {
            return;
        }

        Win32_DestroySwapchainImages(swapchain);

        auto node_masks = NOVA_STACK_ALLOC(UINT, swapchain->image_count);
        auto queues = NOVA_STACK_ALLOC(IUnknown*, swapchain->image_count);
        for (u32 i = 0; i < swapchain->image_count; ++i) {
            node_masks[i] = 1;
            queues[i] = swapchain->dxqueue;
        }

        auto res = swapchain->dxswapchain->ResizeBuffers1(0, width, height, DXGI_FORMAT_UNKNOWN, 0, node_masks, queues);
        if (FAILED(res)) {
            NOVA_THROW("Error resizing swapchain: {} ({:#x})", u32(res), u32(res));
        }

        swapchain->extent = { width, height };

        Win32_GetSwapchainImages(swapchain);
    }

    void Win32_DestroySwapchainImages(Swapchain swapchain)
    {
        for (u32 i = 0; i < swapchain->image_count; ++i) {
            swapchain->context->vkFreeMemory(swapchain->context->device, swapchain->dxmemory[i], swapchain->context->alloc);
            ::CloseHandle(swapchain->dxhandles[i]);
            auto vkimage = swapchain->images[i]->image;
            swapchain->images[i].Destroy();
            swapchain->context->vkDestroyImage(swapchain->context->device, vkimage, swapchain->context->alloc);
        }
    }

    void Win32_GetSwapchainImages(Swapchain swapchain)
    {
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

            // TODO: Required?
            // std::wstring shared_handle_name = std::wstring(L"Local\\SwapchainImage") + std::to_wstring(i);
            NOVA_STACK_POINT();
            auto shared_handle_name = NOVA_STACK_TO_UTF16(NOVA_STACK_FORMAT("Local\\SwapchainImage:{}", (void*)vkimage));

            HANDLE handle = nullptr;
            swapchain->dxdevice->CreateSharedHandle(dx_image, nullptr, GENERIC_ALL, shared_handle_name.data(), &handle);
            swapchain->dxhandles[i] = handle;

            VkMemoryWin32HandlePropertiesKHR win32_mem_props{
                .sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
                .memoryTypeBits = 0xCDCDCDCD,
            };

            vkh::Check(swapchain->vkGetMemoryWin32HandlePropertiesKHR(swapchain->context->device,
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
                handle,
                &win32_mem_props));

            VkMemoryRequirements mem_req;
            swapchain->context->vkGetImageMemoryRequirements(swapchain->context->device, vkimage, &mem_req);

            if (win32_mem_props.memoryTypeBits == 0xCDCDCDCD) {
                // AMD Driver workaround
                win32_mem_props.memoryTypeBits = mem_req.memoryTypeBits;
            } else {
                win32_mem_props.memoryTypeBits &= mem_req.memoryTypeBits;
            }

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

            vkh::Check(swapchain->context->vkAllocateMemory(swapchain->context->device, Temp(VkMemoryAllocateInfo {
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
                image->format = swapchain.Unwrap().GetFormat();
                image->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                image->extent = Vec3(swapchain->extent.width, swapchain->extent.height, 1);
                image->mips = 1;
                image->layers = 1;

                image->image = vkimage;
                vkh::Check(swapchain->context->vkCreateImageView(swapchain->context->device, Temp(VkImageViewCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = vkimage,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = GetVulkanFormat(swapchain->format).vk_format,
                    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
                }), swapchain->context->alloc, &image->view));
            }
        }
    }

    bool Queue::Acquire(Span<HSwapchain> _swapchains, Span<HFence> signals) const
    {
        bool any_resized = false;

        for (auto swapchain : _swapchains) {
            RECT rect;
            GetClientRect(swapchain->dxhwnd, &rect);
            Win32_ResizeSwapchain(swapchain, u32(rect.right), u32(rect.bottom));

            swapchain->index = swapchain->dxswapchain->GetCurrentBackBufferIndex();
        }

        if (signals.size())
        {
            NOVA_STACK_POINT();

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
                .signalSemaphoreInfoCount = u32(signals.size()),
                .pSignalSemaphoreInfos = signal_infos,
            }), nullptr));
            rhi::stats::TimeAdaptingFromAcquire += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        }

        return any_resized;
    }

    void Queue::Present(Span<HSwapchain> _swapchains, Span<HFence> waits, bool host_wait) const
    {
        NOVA_STACK_POINT();

        if (waits.size()) {
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
        }

        for (auto swapchain : _swapchains) {
            // TODO: Parameterize sync interval on present mode
            // TODO: Allow tearing
            swapchain->dxswapchain->Present1(1, 0, Temp(DXGI_PRESENT_PARAMETERS {}));
            if (swapchain->comp_enabled) {
                swapchain->dcomp_visual->SetContent(swapchain->dxswapchain);
                swapchain->dcomp_target->SetRoot(swapchain->dcomp_visual);
                swapchain->dcomp_device->Commit();
            }
        }
        for (auto swapchain : _swapchains) {
            if (swapchain->comp_enabled) {
                swapchain->dcomp_device->WaitForCommitCompletion();
            }
        }
    }

    void CommandList::Present(HSwapchain swapchain) const
    {
        impl->Transition(swapchain.Unwrap().GetCurrent(),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    }
}