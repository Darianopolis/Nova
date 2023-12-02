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
    void Win32_Init(Context context);
    void Win32_CreateSwapchain(Swapchain swapchain, HWND hwnd, u32 width, u32 height);
    void Win32_GetSwapchainImages(Swapchain swapchain);

    Swapchain Swapchain::Create(HContext context, void* window, ImageUsage usage, PresentMode present_mode)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->usage = usage;
        impl->present_mode = present_mode;

        // impl->surface = Platform_CreateVulkanSurface(context, window);

        RECT rect;
        GetClientRect((HWND)window, &rect);

        Win32_Init(context);
        Win32_CreateSwapchain({ impl }, (HWND)window, (u32)rect.right, (u32)rect.bottom);
        Win32_GetSwapchainImages({ impl });

        // std::vector<VkSurfaceFormatKHR> surface_formats;
        // vkh::Enumerate(surface_formats, impl->context->vkGetPhysicalDeviceSurfaceFormatsKHR, context->gpu, impl->surface);

        // for (auto& surface_format : surface_formats) {
        //     if ((surface_format.format == VK_FORMAT_B8G8R8A8_UNORM
        //             || surface_format.format == VK_FORMAT_R8G8B8A8_UNORM)) {
        //         impl->format = surface_format;
        //         break;
        //     }
        // }

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
        return FromVulkanFormat(impl->format.format);
    }

// -----------------------------------------------------------------------------

    namespace {
        IDXGIFactory7* dxfactory = nullptr;
        IDXGIAdapter4* dxadapter = nullptr;
        ID3D12Device5* dxdevice = nullptr;
        ID3D12CommandQueue* dxqueue = nullptr;

        IDXGIDevice* dxgi_device = nullptr;

        IDCompositionDevice* dcomp_device = nullptr;
        IDCompositionVisual* dcomp_visual = nullptr;
        IDCompositionTarget* dcomp_target = nullptr;

        HWND dxhwnd = nullptr;
        IDXGISwapChain4* dxswapchain = nullptr;
        std::vector<VkDeviceMemory> dxmemory;
        std::vector<HANDLE> dxhandles;
    }

    void Win32_Init(Context context)
    {
        CreateDXGIFactory2(0, IID_PPV_ARGS(&dxfactory));

        VkPhysicalDeviceIDProperties id_props { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
        context->vkGetPhysicalDeviceProperties2(context->gpu, Temp(VkPhysicalDeviceProperties2KHR {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &id_props,
        }));

        NOVA_LOGEXPR(id_props.deviceLUIDValid);

        LUID* pluid = reinterpret_cast<LUID*>(&id_props.deviceLUID);

        dxfactory->EnumAdapterByLuid(*pluid, IID_PPV_ARGS(&dxdevice));

        ::D3D12CreateDevice(dxadapter, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&dxdevice));

        dxdevice->CreateCommandQueue(Temp(D3D12_COMMAND_QUEUE_DESC {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 1,
        }), IID_PPV_ARGS(&dxqueue));
        {
            ID3D11Device* d11_device = nullptr;
            D3D11CreateDevice(nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                nullptr,
                0,
                D3D11_SDK_VERSION,
                &d11_device,
                nullptr,
                nullptr);
            d11_device->QueryInterface(&dxgi_device);
            if (!dxgi_device) {
                NOVA_THROW("Failed to query dxgi device");
            }
        }
        DCompositionCreateDevice(dxgi_device, IID_PPV_ARGS(&dcomp_device));
        dcomp_device->CreateVisual(&dcomp_visual);
    }

    void Win32_CreateSwapchain(Swapchain swapchain, HWND hwnd, u32 width, u32 height)
    {
        dcomp_device->CreateTargetForHwnd(hwnd, false, &dcomp_target);

        DXGI_SWAP_CHAIN_DESC1 desc {
            .Width = width,
            .Height = height,
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .Stereo = FALSE,
            .SampleDesc = { 1, 0 },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = 2,
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
            .AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED,
            .Flags = 0,
        };

        dxhwnd = hwnd;

        swapchain->extent.width = width;
        swapchain->extent.height = height;

        IDXGISwapChain1* swapchain1;
        if (auto res = dxfactory->CreateSwapChainForComposition(
                dxqueue, &desc, nullptr, &swapchain1); FAILED(res)) {
            NOVA_LOG("Failed to create swapchain: {:#x}", (u32)res);
            std::terminate();
        }
        swapchain1->QueryInterface(&dxswapchain);
        dxfactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        swapchain->format = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    }

    void Win32_ResizeSwapchain(Swapchain swapchain, u32 width, u32 height)
    {
        ID3D12Resource* image;
        dxswapchain->GetBuffer(0, IID_PPV_ARGS(&image));
        auto desc = image->GetDesc();
        image->Release();
        if (desc.Width == width && desc.Height == height) {
            return;
        }
        // for (u32 i = 0; i < 2; ++i) {
        //     ID3D12Resource* dximage;
        //     dxswapchain->GetBuffer(i, IID_PPV_ARGS(&dximage));
        //     auto counts = dximage->Release();
        //     NOVA_LOG("Remaining count[{}] = {}", i, counts);
        // }
        // NOVA_LOG("Resizing ({}, {}) to ({}, {})", desc.Width, desc.Height, width, height);
        for (u32 i = 0; i < 2; ++i) {
            swapchain->context->vkFreeMemory(swapchain->context->device, dxmemory[i], swapchain->context->alloc);
        }
        for (auto handle : dxhandles) {
            CloseHandle(handle);
        }
        for (auto image : swapchain->images) {
            auto vkimage = image->image;
            image.Destroy();
            swapchain->context->vkDestroyImage(swapchain->context->device, vkimage, swapchain->context->alloc);
        }
        UINT node_masks[2] = { 1, 1 };
        IUnknown* queues[2] = { dxqueue, dxqueue };
        auto res = dxswapchain->ResizeBuffers1(0, width, height, DXGI_FORMAT_UNKNOWN, 0, node_masks, queues);
        swapchain->extent.width = width;
        swapchain->extent.height = height;
        if (FAILED(res)) {
            NOVA_LOG("Error resizing swapchain: {} ({:#x})", u32(res), u32(res));
            std::terminate();
        }
        Win32_GetSwapchainImages(swapchain);
    }

    void Win32_GetSwapchainImages(Swapchain swapchain)
    {
        u32 image_count = 2;
        std::vector<ID3D12Resource*> dx_images(image_count);
        for (u32 i = 0; i < image_count; ++i) {
            dxswapchain->GetBuffer(i, IID_PPV_ARGS(&dx_images[i]));
        }

        swapchain->images.resize(image_count);
        dxhandles.resize(image_count);
        dxmemory.resize(image_count);

        for (u32 i = 0; i < image_count; ++i) {
            auto* dx_image = dx_images[i];
            // NOVA_LOGEXPR(dx_image);
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
                .usage = GetVulkanImageUsage(swapchain->usage),
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            };

            VkImage vkimage;
            vkh::Check(swapchain->context->vkCreateImage(swapchain->context->device, &ii, swapchain->context->alloc, &vkimage));

            // TODO: Required?
            std::wstring shared_handle_name = std::wstring(L"Local\\SwapchainImage{}") + std::to_wstring(i);
            HANDLE handle = nullptr;
            dxdevice->CreateSharedHandle(dx_image, nullptr, GENERIC_ALL, shared_handle_name.c_str(), &handle);
            dxhandles[i] = handle;

            VkMemoryWin32HandlePropertiesKHR win32_mem_props{
                .sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
                .memoryTypeBits = 0xCDCDCDCD,
            };

#define NOVA_LOAD(context, function)

            auto vkGetMemoryWin32HandlePropertiesKHR = (PFN_vkGetMemoryWin32HandlePropertiesKHR)swapchain->context->vkGetInstanceProcAddr(swapchain->context->instance, "vkGetMemoryWin32HandlePropertiesKHR");

            // NOVA_LOGEXPR(vkGetMemoryWin32HandlePropertiesKHR);
            // NOVA_LOGEXPR(handle);

            vkh::Check(vkGetMemoryWin32HandlePropertiesKHR(swapchain->context->device,
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

            dxmemory[i] = memory;

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
                    .format = swapchain->format.format,
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
            GetClientRect(dxhwnd, &rect);
            Win32_ResizeSwapchain(swapchain, u32(rect.right), u32(rect.bottom));

            swapchain->index = dxswapchain->GetCurrentBackBufferIndex();
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
            dxswapchain->Present1(1, 0, Temp(DXGI_PRESENT_PARAMETERS {}));
            dcomp_visual->SetContent(dxswapchain);
            dcomp_target->SetRoot(dcomp_visual);
            dcomp_device->Commit();
            dcomp_device->WaitForCommitCompletion();
        }
    }

    void CommandList::Present(HSwapchain swapchain) const
    {
        impl->Transition(swapchain.Unwrap().GetCurrent(),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    }
}