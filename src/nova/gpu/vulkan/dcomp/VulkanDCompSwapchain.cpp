#if 0
#include "nova_VulkanDCompSwapchain.hpp"

#include <nova/window/Window.hpp>

namespace nova
{
    struct DCompSwapchainStrategy : SwapchainStrategy
    {
        virtual void Destroy(Swapchain swapchain) final override
        {
            if (!swapchain) {
                return;
            }

            auto* impl = static_cast<DCompSwapchainData*>(swapchain.operator->());

            delete impl;
        }

        virtual Image Target(Swapchain swapchain) final override
        {
            auto* data = DCompSwapchainData::Get(swapchain);
            return data->buffers[data->buffer_index].image;
        }

        virtual Vec2U Extent(Swapchain swapchain) final override
        {
            auto* data = DCompSwapchainData::Get(swapchain);
            return { data->width, data->height };
        }

        virtual nova::Format Format(Swapchain swapchain) final override
        {
            return nova::Format::BGRA8_UNorm;
        }

        virtual SyncPoint Acquire(Queue queue, Span<HSwapchain> swapchains, bool* out_any_resized) final override
        {

        }

        virtual void PreparePresent(CommandList cmd, HSwapchain swapchain) final override
        {

        }

        virtual void Present(Queue queue, Span<HSwapchain> swapchains, Span<SyncPoint> waits, PresentFlag flags) final override
        {

        }

        void InitSwapchain(DCompSwapchainData* swapchain)
        {
            // TODO: Cleanup on errors

            // D3D11 Device

            ID3D11Device* d3d11_device_1 = {};
            win::Check(D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
                nullptr,
                0,
                D3D11_SDK_VERSION,
                &d3d11_device_1,
                nullptr,
                nullptr));
            win::Check(d3d11_device_1->QueryInterface(&swapchain->d3d11_device));

            // Composition Device

            {
                IDXGIDevice* dxgi_device = {};
                win::Check(swapchain->d3d11_device->QueryInterface(&dxgi_device));
                NOVA_DEFER(&) { dxgi_device->Release(); };

                win::Check(DCompositionCreateDevice(dxgi_device, IID_PPV_ARGS(&swapchain->comp_device)));
            }

            // Presentation Manager

            {
                IPresentationFactory* presentation_factory;
                win::Check(CreatePresentationFactory(swapchain->d3d11_device, IID_PPV_ARGS(&presentation_factory)));
                NOVA_DEFER(&) { presentation_factory->Release(); };

                if (!presentation_factory->IsPresentationSupported()) {
                    NOVA_THROW("Presentation not supported on presentation factory");
                }

                win::Check(presentation_factory->CreatePresentationManager(&swapchain->manager));
            }

            //  Presentation Surface, Composition Surface

            win::Check(DCompositionCreateSurfaceHandle(COMPOSITIONOBJECT_ALL_ACCESS, nullptr, &swapchain->surface_handle));
            win::Check(swapchain->manager->CreatePresentationSurface(swapchain->surface_handle, &swapchain->surface));
            win::Check(swapchain->comp_device->CreateSurfaceFromHandle(swapchain->surface_handle, &swapchain->comp_surface));

            // Composition Target, Composition Visual

            win::Check(swapchain->comp_device->CreateTargetForHwnd((HWND)swapchain->window.NativeHandle(), true, &swapchain->comp_target));
            win::Check(swapchain->comp_device->CreateVisual(&swapchain->comp_visual));
            win::Check(swapchain->comp_visual->SetContent(swapchain->comp_surface));
            win::Check(swapchain->comp_target->SetRoot(swapchain->comp_visual));
            win::Check(swapchain->comp_device->Commit());
            win::Check(swapchain->comp_device->WaitForCommitCompletion()); // Not really necessary

            // Get events

            win::Check(swapchain->manager->GetLostEvent(&swapchain->lost_event));
            win::Check(swapchain->manager->GetPresentRetiringFence(IID_PPV_ARGS(&swapchain->retire_fence)));
            swapchain->terminate_event = CreateEventW(nullptr, true, false, nullptr);
            swapchain->retire_event = CreateEventW(nullptr, true, false, nullptr);
            for (u32 i = 0; i < swapchain->buffer_count; ++i) {
                swapchain->events.emplace_back(CreateEventW(nullptr, true, false, nullptr));
            }

            // Vulkan functions

            if (!(swapchain->vkGetMemoryWin32HandlePropertiesKHR = (PFN_vkGetMemoryWin32HandlePropertiesKHR)
                    swapchain->context->vkGetInstanceProcAddr(swapchain->context->instance, "vkGetMemoryWin32HandlePropertiesKHR"))) {
                NOVA_THROW("DComp Swapchain :: Failed to load vkGetMemoryWin32HandlePropertiesKHR");
            }

            if (!(swapchain->vkImportSemaphoreWin32HandleKHR = (PFN_vkImportSemaphoreWin32HandleKHR)
                    swapchain->context->vkGetInstanceProcAddr(swapchain->context->instance, "vkImportSemaphoreWin32HandleKHR"))) {
                NOVA_THROW("DComp Swapchain :: Failed to load vkImportSemaphoreWin32HandleKHR");
            }
        }

        void CreateBuffer(DCompSwapchainData* swapchain, u32 index)
        {
            IDXGIResource1* dxgi_resource = {}; // ??
            D3D11_TEXTURE2D_DESC desc {
                .Width = swapchain->width,
                .Height = swapchain->height,
                .MipLevels = 1,
                .ArraySize = 1,
                .Format = swapchain->format,
                .SampleDesc = { 1, 0 },
                .Usage = D3D11_USAGE_DEFAULT,
                .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
                .CPUAccessFlags = 0,
                .MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE,
            };

            auto& buffer = swapchain->buffers[index];

            win::Check(swapchain->d3d11_device->CreateTexture2D(&desc, nullptr, &buffer.texture));
            win::Check(swapchain->manager->AddBufferFromResource(buffer.texture, &buffer.presentation_buffer));
            win::Check(buffer.texture->QueryInterface(&dxgi_resource)); // ??
            win::Check(dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &buffer.texture_handle));

            auto& context = swapchain->context;
            auto& device = context->device;

            // VkMemoryWin32HandlePropertiesKHR handle_props {
            //     .sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
            // };
            // vkh::Check(swapchain->vkGetMemoryWin32HandlePropertiesKHR(device,
            //     VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT, buffer.texture_handle, &handle_props));

            VkImage vkimage;
            vkh::Check(context->vkCreateImage(device, PtrTo(VkImageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext = PtrTo(VkExternalMemoryImageCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
                    .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,
                }),
                .imageType = VK_IMAGE_TYPE_2D,
                .format = VK_FORMAT_B8G8R8A8_UNORM,
                .extent = { u32(swapchain->width), u32(swapchain->height), 1 },
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = GetVulkanImageUsage(swapchain->usage),
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = context->queue_family_count,
                .pQueueFamilyIndices = context->queue_families.data(),
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            }), context->alloc, &vkimage));

            VkMemoryWin32HandlePropertiesKHR win32_mem_props{
                .sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
                .memoryTypeBits = ~0u,
            };

            vkh::Check(swapchain->vkGetMemoryWin32HandlePropertiesKHR(swapchain->context->device,
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,
                buffer.texture_handle,
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
                .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,
                .handle = buffer.texture_handle,
                .name = nullptr,
            };

            vkh::Check(swapchain->context->vkAllocateMemory(swapchain->context->device, PtrTo(VkMemoryAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .pNext = &imi,
                .allocationSize = mem_req.size,
                .memoryTypeIndex = mem_type_index,
            }), swapchain->context->alloc, &buffer.memory));

            vkh::Check(swapchain->context->vkBindImageMemory(swapchain->context->device, vkimage, buffer.memory, 0));

            {
                auto& image = buffer.image;
                image->context = swapchain->context;

                image->usage = swapchain->usage;
                image->format = nova::Format::BGRA8_UNorm;
                image->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                image->extent = Vec3(swapchain->width, swapchain->height, 1);
                image->mips = 1;
                image->layers = 1;

                image->image = vkimage;
                vkh::Check(swapchain->context->vkCreateImageView(swapchain->context->device, PtrTo(VkImageViewCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = vkimage,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = GetVulkanFormat(image->format).vk_format,
                    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
                }), swapchain->context->alloc, &image->view));
            }

            // -- Import semaphore

            buffer.fence = nova::Fence::Create(context);

            vkh::Check(swapchain->vkImportSemaphoreWin32HandleKHR(device, PtrTo(VkImportSemaphoreWin32HandleInfoKHR {
                .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
                .semaphore = buffer.fence->semaphore,
                .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D11_FENCE_BIT,
                .handle = buffer.present_fence_handle,
            })));

            buffer.state = 0;

            swapchain->present_queue[index] = ~0u; // Initialize all present queue entries to ~0u signifying no presents are made
        }
    };

    DCompSwapchainStrategy* GetDCompSwapchainStrategy()
    {
        static DCompSwapchainStrategy strategy;
        return &strategy;
    }

    Swapchain DCompSwapchain_Create(HContext context, Window window, ImageUsage usage, PresentMode present_mode)
    {
        auto strategy = GetDCompSwapchainStrategy();
        auto impl = new DCompSwapchainData;
        impl->strategy = strategy;
        impl->context = context;
        impl->usage = usage;

        Swapchain swapchain = { impl };

        strategy->InitSwapchain(impl);

        return swapchain;
    }
}
#endif
