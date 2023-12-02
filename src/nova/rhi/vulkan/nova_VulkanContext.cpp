#include "nova_VulkanRHI.hpp"

#include <nova/core/nova_Stack.hpp>
#include <nova/core/nova_ToString.hpp>

namespace nova
{
    static
    VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT  severity,
        VkDebugUtilsMessageTypeFlagsEXT             type,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        [[maybe_unused]] void*                  userdata)
    {
        if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                || type != VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            return VK_FALSE;
        }

        NOVA_LOG(R"(
────────────────────────────────────────────────────────────────────────────────)");
        std::cout << std::stacktrace::current();
        NOVA_LOG(R"(
────────────────────────────────────────────────────────────────────────────────
Validation-VUID({}): {}
────────────────────────────────────────────────────────────────────────────────
{}
────────────────────────────────────────────────────────────────────────────────
)",
            data->messageIdNumber,
            data->pMessageIdName,
            data->pMessage);

        std::terminate();
    }

    struct VulkanFeatureChain
    {
        HashMap<VkStructureType, VkBaseInStructure*> device_features;
        ankerl::unordered_dense::set<std::string>         extensions;
        VkBaseInStructure*                                      next = nullptr;

        ~VulkanFeatureChain()
        {
            for (auto&[type, feature] : device_features) {
                std::free(feature);
            }
        }

        void Extension(std::string name)
        {
            extensions.emplace(std::move(name));
        }

        template<typename T>
        T& Feature(VkStructureType type)
        {
            auto& f = device_features[type];
            if (!f) {
                f = static_cast<VkBaseInStructure*>(std::malloc(sizeof(T)));
                new(f) T{};
                f->sType = type;
                f->pNext = next;
                next = f;
            }

            return *(T*)f;
        }

        const void* Build()
        {
            return next;
        }
    };

    Context Context::Create(const ContextConfig& config)
    {
        NOVA_STACK_POINT();

        auto impl = new Impl;
        impl->config = config;

        // Configure instance layers and validation features

        std::vector<const char*> instance_layers;
        std::vector<VkValidationFeatureEnableEXT> validation_features_enabled;

        if (config.debug) {
            instance_layers.push_back("VK_LAYER_KHRONOS_validation");
        }

        if (config.api_dump) {
            instance_layers.push_back("VK_LAYER_LUNARG_api_dump");
        }

        std::vector<const char*> instance_extensions = { VK_KHR_SURFACE_EXTENSION_NAME };
#ifdef _WIN32
        instance_extensions.push_back("VK_KHR_win32_surface");
#endif
        if (config.debug) {
            instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            instance_extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        }

        if (config.extra_validation) {
            validation_features_enabled.push_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
            validation_features_enabled.push_back(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
        }

        // Load pre-instance functions

        impl->vkGetInstanceProcAddr = Platform_LoadGetInstanceProcAddr();

#define NOVA_VULKAN_FUNCTION(name) {                                           \
    auto pfn = (PFN_##name)impl->vkGetInstanceProcAddr(nullptr, #name);        \
    if (pfn) impl->name = pfn;                                                 \
}
#include "nova_VulkanFunctions.inl"

        // Create instance

        if (config.trace) {
            uint32_t instance_layer_count = NULL;
            auto result = impl->vkEnumerateInstanceLayerProperties(&instance_layer_count,nullptr);
            std::vector<VkLayerProperties> instance_layers(instance_layer_count);
            result = impl->vkEnumerateInstanceLayerProperties(&instance_layer_count,&instance_layers[0]);

            for (u32 i = 0; i < instance_layer_count; ++i) {
                NOVA_LOG("Instance layer[{}] = {}", i, instance_layers[i].layerName);
            }
        }

        VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DebugCallback,
            .pUserData = impl,
        };

        vkh::Check(impl->vkCreateInstance(Temp(VkInstanceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = config.debug
                ? Temp(VkValidationFeaturesEXT {
                    .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
                    .pNext = &debug_messenger_info,
                    .enabledValidationFeatureCount = u32(validation_features_enabled.size()),
                    .pEnabledValidationFeatures = validation_features_enabled.data(),
                })
                : nullptr,
            .pApplicationInfo = Temp(VkApplicationInfo {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .apiVersion = VK_API_VERSION_1_3,
            }),
            .enabledLayerCount = u32(instance_layers.size()),
            .ppEnabledLayerNames = instance_layers.data(),
            .enabledExtensionCount = u32(instance_extensions.size()),
            .ppEnabledExtensionNames = instance_extensions.data(),
        }), impl->alloc, &impl->instance));

        // Load instance functions

#define NOVA_VULKAN_FUNCTION(name) {                                           \
    auto pfn = (PFN_##name)impl->vkGetInstanceProcAddr(impl->instance, #name); \
    if (pfn) impl->name = pfn;                                                 \
}
#include "nova_VulkanFunctions.inl"

        // Create debug messenger

        if (config.debug) {
            vkh::Check(impl->vkCreateDebugUtilsMessengerEXT(impl->instance, &debug_messenger_info, impl->alloc, &impl->debug_messenger));
        }

        // Select physical device

        std::vector<VkPhysicalDevice> gpus;
        vkh::Enumerate(gpus, impl->vkEnumeratePhysicalDevices, impl->instance);
        for (auto& gpu : gpus) {
            VkPhysicalDeviceProperties2 properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
            impl->vkGetPhysicalDeviceProperties2(gpu, &properties);
            if (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                impl->gpu = gpu;

                break;
            }
        }

        if (!impl->gpu) {
            NOVA_THROW("No suitable physical device found");
        }

        // Configure features

        VulkanFeatureChain chain;

        {
            // Swapchains

            chain.Extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

            // Core Features

            {
                auto& f = chain.Feature<VkPhysicalDeviceFeatures2>(
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
                f.features.wideLines = VK_TRUE;
                f.features.shaderInt16 = VK_TRUE;
                f.features.shaderInt64 = VK_TRUE;
                f.features.fillModeNonSolid = VK_TRUE;
                f.features.samplerAnisotropy = VK_TRUE;
                f.features.multiDrawIndirect = VK_TRUE;
                f.features.independentBlend = VK_TRUE;
                f.features.imageCubeArray = VK_TRUE;
                f.features.drawIndirectFirstInstance = VK_TRUE;
                f.features.fragmentStoresAndAtomics = VK_TRUE;
                f.features.multiViewport = VK_TRUE;
            }

            {
                // External memory imports (for DXGI interop)

                chain.Extension(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
                chain.Extension("VK_KHR_external_memory_win32");
            }

            {
                auto& f = chain.Feature<VkPhysicalDeviceVulkan11Features>(
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
                f.storagePushConstant16 = VK_TRUE;
                f.storageBuffer16BitAccess = VK_TRUE;
                f.shaderDrawParameters = VK_TRUE;
                f.multiview = VK_TRUE;
                f.multiviewGeometryShader = VK_TRUE;
                f.multiviewTessellationShader = VK_TRUE;
            }

            // Vulkan 1.2

            {
                auto& f = chain.Feature<VkPhysicalDeviceVulkan12Features>(
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
                f.drawIndirectCount = VK_TRUE;
                f.shaderInt8 = VK_TRUE;
                f.shaderFloat16 = VK_TRUE;
                f.timelineSemaphore = VK_TRUE;
                f.scalarBlockLayout = VK_TRUE;
                f.descriptorIndexing = VK_TRUE;
                f.samplerFilterMinmax = VK_TRUE;
                f.bufferDeviceAddress = VK_TRUE;
                f.imagelessFramebuffer = VK_TRUE;
                f.storagePushConstant8 = VK_TRUE;
                f.runtimeDescriptorArray = VK_TRUE;
                f.storageBuffer8BitAccess = VK_TRUE;
                f.descriptorBindingPartiallyBound = VK_TRUE;
                f.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
                f.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
                f.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
                f.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
                f.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
                f.shaderInputAttachmentArrayNonUniformIndexing = VK_TRUE;
                f.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
                f.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
                f.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
                f.descriptorBindingVariableDescriptorCount = VK_TRUE;
            }

            // Vulkan 1.3

            {
                auto& f = chain.Feature<VkPhysicalDeviceVulkan13Features>(
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
                f.maintenance4 = VK_TRUE;
                f.dynamicRendering = VK_TRUE;
                f.synchronization2 = VK_TRUE;
            }

            // Host Image Copy

            if (!config.compatibility) {
                chain.Extension(VK_EXT_HOST_IMAGE_COPY_EXTENSION_NAME);
                chain.Feature<VkPhysicalDeviceHostImageCopyFeaturesEXT>(
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT)
                    .hostImageCopy = VK_TRUE;
                impl->transfer_manager.staged_image_copy = false;
            }

#if 0
            // Shader Objects

            chain.Extension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceShaderObjectFeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT)
                .shaderObject = VK_TRUE;
            impl->shaderObjects = true;
#endif

#if 0
            // Descriptor Buffers

            {
                chain.Extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
                auto& f = chain.Feature<VkPhysicalDeviceDescriptorBufferFeaturesEXT>(
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT);
                f.descriptorBuffer = VK_TRUE;
                f.descriptorBufferPushDescriptors = VK_TRUE;
                impl->descriptorBuffers = true;
            }
#endif

            // Graphics Pipeline Libraries + Extended Dynamic State

            chain.Feature<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT)
                .extendedDynamicState = VK_TRUE;

            chain.Feature<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT)
                .extendedDynamicState2 = VK_TRUE;

            chain.Extension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
            chain.Extension(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT)
                .graphicsPipelineLibrary = VK_TRUE;

            // Fragment Shader Barycentrics

            chain.Extension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR)
                .fragmentShaderBarycentric = VK_TRUE;

#if 0
            // Fragment Shader Interlock

            {
                chain.Extension(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME);
                auto& f = chain.Feature<VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT>(
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT);
                f.fragmentShaderPixelInterlock = VK_TRUE;
                f.fragmentShaderSampleInterlock = VK_TRUE;
                f.fragmentShaderShadingRateInterlock = VK_TRUE;
            }
#endif
        }

        if (config.mesh_shading) {
            // Mesh Shading extensions

            chain.Extension(VK_EXT_MESH_SHADER_EXTENSION_NAME);
            auto& f = chain.Feature<VkPhysicalDeviceMeshShaderFeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT);
            f.meshShader = true;
            f.meshShaderQueries = true;
            f.multiviewMeshShader = true;
            f.taskShader = true;
        }

        if (config.ray_tracing) {

            // Ray Tracing extensions

            chain.Extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
            chain.Extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
            chain.Extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR)
                .rayTracingPipeline = VK_TRUE;
            chain.Feature<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR)
                .accelerationStructure = VK_TRUE;

            chain.Extension(VK_KHR_RAY_QUERY_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceRayQueryFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR)
                .rayQuery = VK_TRUE;

            chain.Extension(VK_NV_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV)
                .rayTracingInvocationReorder = VK_TRUE;

            chain.Extension(VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR)
                .rayTracingPositionFetch = VK_TRUE;
        }

        // TODO: Move this into VulkanFeatureChain
        auto device_extensions = NOVA_STACK_ALLOC(const char*, chain.extensions.size());
        {
            u32 i = 0;
            for (const auto& ext : chain.extensions) {
                device_extensions[i++] = ext.c_str();
            }
        }

        {
            std::vector<VkExtensionProperties> props;
            vkh::Enumerate(props, impl->vkEnumerateDeviceExtensionProperties, impl->gpu, nullptr);

            std::unordered_set<std::string_view> supported;
            for (auto& prop : props) {
                supported.insert(prop.extensionName);
            }

            u32 missing_count = 0;
            for (auto& ext : chain.extensions) {
                if (!supported.contains(ext)) {
                    missing_count++;
                    NOVA_LOG("Missing extension: {}", ext);
                }
            }

            if (missing_count) {
                NOVA_LOG("Missing {} extension{}.\nPress Enter to close...",
                    missing_count, missing_count > 1 ? "s" : "");
                // Log to file to avoid needing this?
                std::getline(std::cin, *Temp(std::string{}));
                NOVA_THROW("Missing {} extensions.", missing_count);
            }
        }

        // Configure queues
        // TODO: This is bad and you should feel bad. Fix it now.

        std::array<VkQueueFamilyProperties, 3> properties;
        impl->vkGetPhysicalDeviceQueueFamilyProperties(impl->gpu, Temp(3u), properties.data());
        for (u32 i = 0; i < 16; ++i) {
            auto queue = new Queue::Impl;
            queue->context = { impl };
            queue->stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            queue->family = 0;
            queue->flags = properties[0].queueFlags;
            impl->graphic_queues.emplace_back(queue);
        }
        for (u32 i = 0; i < 2; ++i) {
            auto queue = new Queue::Impl;
            queue->context = { impl };
            queue->stages = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            queue->family = 1;
            queue->flags = properties[1].queueFlags;
            impl->transfer_queues.emplace_back(queue);
        }
        for (u32 i = 0; i < 8; ++i) {
            auto queue = new Queue::Impl;
            queue->context = { impl };
            queue->stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            queue->family = 2;
            queue->flags = properties[2].queueFlags;
            impl->compute_queues.emplace_back(queue);
        }

        auto priorities = NOVA_STACK_ALLOC(float, impl->graphic_queues.size());
        for (u32 i = 0; i < impl->graphic_queues.size(); ++i) {
            priorities[i] = 1.f;
        }

        auto low_priorities = NOVA_STACK_ALLOC(float, 8);
        for (u32 i = 0; i < 8; ++i) {
            low_priorities[i] = 0.1f;
        }

        // Create device

        vkh::Check(impl->vkCreateDevice(impl->gpu, Temp(VkDeviceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = chain.Build(),
            .queueCreateInfoCount = 3,
            .pQueueCreateInfos = std::array {
                VkDeviceQueueCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = impl->graphic_queues.front()->family,
                    .queueCount = u32(impl->graphic_queues.size()),
                    .pQueuePriorities = priorities,
                },
                VkDeviceQueueCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = impl->transfer_queues.front()->family,
                    .queueCount = u32(impl->transfer_queues.size()),
                    .pQueuePriorities = low_priorities,
                },
                VkDeviceQueueCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = impl->compute_queues.front()->family,
                    .queueCount = u32(impl->compute_queues.size()),
                    .pQueuePriorities = low_priorities,
                },
            }.data(),
            .enabledExtensionCount = u32(chain.extensions.size()),
            .ppEnabledExtensionNames = device_extensions,
        }), impl->alloc, &impl->device));

        // Load device functions

#define NOVA_VULKAN_FUNCTION(name) {                                           \
    auto pfn = (PFN_##name)impl->vkGetDeviceProcAddr(impl->device, #name);     \
    if (pfn) impl->name = pfn;                                                 \
}
#include "nova_VulkanFunctions.inl"

        // Query device properties

        impl->vkGetPhysicalDeviceProperties2(impl->gpu, Temp(VkPhysicalDeviceProperties2 {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &impl->descriptor_sizes,
        }));

        // Get queues

        for (u32 i = 0; i < impl->graphic_queues.size(); ++i) {
            impl->vkGetDeviceQueue(impl->device, impl->graphic_queues[i]->family, i, &impl->graphic_queues[i]->handle);
        }

        for (u32 i = 0; i < impl->transfer_queues.size(); ++i) {
            impl->vkGetDeviceQueue(impl->device, impl->transfer_queues[i]->family, i, &impl->transfer_queues[i]->handle);
        }

        for (u32 i = 0; i < impl->compute_queues.size(); ++i) {
            impl->vkGetDeviceQueue(impl->device, impl->compute_queues[i]->family, i, &impl->compute_queues[i]->handle);
        }

        // Create VMA allocator

        vkh::Check(vmaCreateAllocator(Temp(VmaAllocatorCreateInfo {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = impl->gpu,
            .device = impl->device,
            .pAllocationCallbacks = impl->alloc,
            .pVulkanFunctions = Temp(VmaVulkanFunctions {
                .vkGetInstanceProcAddr = impl->vkGetInstanceProcAddr,
                .vkGetDeviceProcAddr = impl->vkGetDeviceProcAddr,
            }),
            .instance = impl->instance,
            .vulkanApiVersion = VK_API_VERSION_1_3,
        }), &impl->vma));

        // Create pipeline cache

        vkh::Check(impl->vkCreatePipelineCache(impl->device, Temp(VkPipelineCacheCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .initialDataSize = 0,
        }), impl->alloc, &impl->pipeline_cache));

        // Create descriptor heap

        {
            constexpr u32 NumImageDescriptors = 1024 * 1024;
            constexpr u32 NumSamplerDescriptors = 4096;
            impl->global_heap.Init(impl, NumImageDescriptors, NumSamplerDescriptors);
        }

        // Create transfer manager

        impl->transfer_manager.Init(impl);

        return { impl };
    }

    void Context::Destroy()
    {
        if (!impl) {
            return;
        }

        WaitIdle();

        for (auto& queue : impl->graphic_queues)  { delete queue.impl; }
        for (auto& queue : impl->compute_queues)  { delete queue.impl; }
        for (auto& queue : impl->transfer_queues) { delete queue.impl; }

        // Deleted graphics pipeline library stages
        for (auto&[key, pipeline] : impl->vertex_input_stages)    { impl->vkDestroyPipeline(impl->device, pipeline, impl->alloc); }
        for (auto&[key, pipeline] : impl->preraster_stages)       { impl->vkDestroyPipeline(impl->device, pipeline, impl->alloc); }
        for (auto&[key, pipeline] : impl->fragment_shader_stages) { impl->vkDestroyPipeline(impl->device, pipeline, impl->alloc); }
        for (auto&[key, pipeline] : impl->fragment_output_stages) { impl->vkDestroyPipeline(impl->device, pipeline, impl->alloc); }
        for (auto&[key, pipeline] : impl->graphics_pipeline_sets) { impl->vkDestroyPipeline(impl->device, pipeline, impl->alloc); }
        for (auto&[key, pipeline] : impl->compute_pipelines)      { impl->vkDestroyPipeline(impl->device, pipeline, impl->alloc); }

        impl->global_heap.Destroy();
        impl->transfer_manager.Destroy();

        // Destroy context vk objects
        impl->vkDestroyPipelineCache(impl->device, impl->pipeline_cache, impl->alloc);
        vmaDestroyAllocator(impl->vma);
        impl->vkDestroyDevice(impl->device, impl->alloc);
        if (impl->debug_messenger) {
            impl->vkDestroyDebugUtilsMessengerEXT(impl->instance, impl->debug_messenger, impl->alloc);
        }
        impl->vkDestroyInstance(impl->instance, impl->alloc);

        if (rhi::stats::MemoryAllocated > 0 || rhi::stats::AllocationCount > 0) {
            NOVA_LOG("WARNING: {} memory allocation ({}) remaining. Improper cleanup",
                rhi::stats::AllocationCount.load(), ByteSizeToString(rhi::stats::MemoryAllocated.load()));
        }

        delete impl;
        impl = nullptr;
    }

    void Context::WaitIdle() const
    {
        impl->vkDeviceWaitIdle(impl->device);
    }

    const ContextConfig& Context::GetConfig() const
    {
        return impl->config;
    }

    void* VulkanTrackedAllocate(void*, size_t size, size_t align, VkSystemAllocationScope)
    {
        align = std::max(8ull, align);

        void* ptr = _aligned_offset_malloc(size + 8, align, 8);

        if (ptr) {
            static_cast<usz*>(ptr)[0] = size;

            rhi::stats::MemoryAllocated += size;
            rhi::stats::AllocationCount++;
            rhi::stats::NewAllocationCount++;

#ifdef NOVA_RHI_NOISY_ALLOCATIONS
            NOVA_LOG("Allocated    {}, size = {}", (void*)ByteOffsetPointer(ptr, 8), size);
#endif
        }

        return ByteOffsetPointer(ptr, 8);
    }

    void* VulkanTrackedReallocate(void*, void* orig, size_t size, size_t align, VkSystemAllocationScope)
    {
        align = std::max(8ull, align);

        if (orig) {
            usz old_size  = static_cast<usz*>(orig)[-1];

#ifdef NOVA_RHI_NOISY_ALLOCATIONS
            NOVA_LOG("Reallocating {}, size = {}/{}", orig, old_size, size);
#endif

            rhi::stats::MemoryAllocated -= old_size;
            rhi::stats::AllocationCount--;
        }

        void* ptr = _aligned_offset_realloc(ByteOffsetPointer(orig, -8), size + 8, align, 8);
        static_cast<usz*>(ptr)[0] = size;

        if (ptr) {
            rhi::stats::MemoryAllocated += size;
            rhi::stats::AllocationCount++;
            if (ptr != orig) {
                rhi::stats::NewAllocationCount++;
            }
        }

        return ByteOffsetPointer(ptr, 8);
    }

    void VulkanTrackedFree(void*, void* ptr)
    {
        if (ptr) {
            usz size = static_cast<usz*>(ptr)[-1];

#ifdef NOVA_RHI_NOISY_ALLOCATIONS
            NOVA_LOG("Freeing      {}, size = {}", ptr, size);
#endif

            rhi::stats::MemoryAllocated -= size;
            rhi::stats::AllocationCount--;

            ptr = ByteOffsetPointer(ptr, -8);
        }

        _aligned_free(ptr);
    }

    void VulkanNotifyAllocation(void*, size_t size, VkInternalAllocationType, VkSystemAllocationScope)
    {
#ifdef NOVA_RHI_NOISY_ALLOCATIONS
        NOVA_LOG("Internal allocation of size {}, type = {}", size, int(type));
#endif
        rhi::stats::MemoryAllocated += size;
        rhi::stats::AllocationCount++;
        rhi::stats::NewAllocationCount++;

    }

    void VulkanNotifyFree(void*, size_t size, VkInternalAllocationType, VkSystemAllocationScope)
    {
#ifdef NOVA_RHI_NOISY_ALLOCATIONS
        NOVA_LOG("Internal free of size {}, type = {}", size, int(type));
#endif
        rhi::stats::MemoryAllocated -= size;
        rhi::stats::AllocationCount--;
    }
}