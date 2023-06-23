#include "nova_VulkanContext.hpp"

namespace nova
{
    std::atomic_int64_t VulkanContext::AllocationCount = 0;
    std::atomic_int64_t VulkanContext::NewAllocationCount = 0;

    static
    VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        [[maybe_unused]] void* userData)
    {
        if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT || type != VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
            return VK_FALSE;

        NOVA_LOG(R"(
--------------------------------------------------------------------------------)");
        std::cout << std::stacktrace::current();
        NOVA_LOG(R"(
--------------------------------------------------------------------------------
Validation: {} ({})
--------------------------------------------------------------------------------
{}
--------------------------------------------------------------------------------
)",
            data->pMessageIdName,
            data->messageIdNumber,
            data->pMessage);

        return VK_FALSE;
    }

    struct VulkanFeatureChain
    {
        ankerl::unordered_dense::map<VkStructureType, std::any> deviceFeatures;
        ankerl::unordered_dense::set<std::string> extensions;
        void* pNext = nullptr;

        VulkanFeatureChain()
        {
            auto& feature = deviceFeatures[VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2];
            auto& f = feature.emplace<VkPhysicalDeviceFeatures2>();
            f.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        }

        void Extension(std::string name)
        {
            extensions.emplace(std::move(name));
        }

        template<class T>
        T& Feature(VkStructureType type)
        {
            auto& feature = deviceFeatures[type];
            if (!feature.has_value()) {
                auto& f = feature.emplace<T>();
                f.sType = type;
                f.pNext = pNext;
                pNext = &f;
            }

            return std::any_cast<T&>(feature);
        }

        const void* Build()
        {
            auto& f2 = std::any_cast<VkPhysicalDeviceFeatures2&>(deviceFeatures.at(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2));
            f2.pNext = pNext;
            return &f2;
        }
    };

    VulkanContext::VulkanContext(const ContextConfig& _context)
        : config(_context)
    {
        std::vector<const char*> instanceLayers;
        if (config.debug)
            instanceLayers.push_back("VK_LAYER_KHRONOS_validation");

        std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };
    #ifdef _WIN32
        instanceExtensions.push_back("VK_KHR_win32_surface");
    #endif
        if (config.debug)
        {
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            instanceExtensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        }

        volkInitialize();

        VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DebugCallback,
            .pUserData = this,
        };

        std::vector<VkValidationFeatureEnableEXT> validationFeaturesEnabled {
            // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
            // VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
            // VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
        };

        VkValidationFeaturesEXT validationFeatures {
            .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
            .pNext = &debugMessengerCreateInfo,
            .enabledValidationFeatureCount = u32(validationFeaturesEnabled.size()),
            .pEnabledValidationFeatures = validationFeaturesEnabled.data(),
        };

        VkCall(vkCreateInstance(Temp(VkInstanceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = config.debug ? &validationFeatures : nullptr,
            .pApplicationInfo = Temp(VkApplicationInfo {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .apiVersion = VK_API_VERSION_1_3,
            }),
            .enabledLayerCount = u32(instanceLayers.size()),
            .ppEnabledLayerNames = instanceLayers.data(),
            .enabledExtensionCount = u32(instanceExtensions.size()),
            .ppEnabledExtensionNames = instanceExtensions.data(),
        }), pAlloc, &instance));

        volkLoadInstanceOnly(instance);

        if (config.debug)
            VkCall(vkCreateDebugUtilsMessengerEXT(instance, &debugMessengerCreateInfo, pAlloc, &debugMessenger));

        std::vector<VkPhysicalDevice> gpus;
        VkQuery(gpus, vkEnumeratePhysicalDevices, instance);
        for (auto& _gpu : gpus)
        {
            VkPhysicalDeviceProperties2 properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
            vkGetPhysicalDeviceProperties2(_gpu, &properties);
            if (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                gpu = _gpu;
                break;
            }
        }

        // ---- Logical Device ----

        vkGetPhysicalDeviceQueueFamilyProperties2(gpu, Temp(0u), nullptr);
        graphics = queues.Acquire().first;
        Get(graphics).family = 0;

        VulkanFeatureChain chain;

        // TODO: Allow for optional features

        {
            chain.Extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            chain.Extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

            auto& f2 = chain.Feature<VkPhysicalDeviceFeatures2>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
            f2.features.wideLines = VK_TRUE;
            f2.features.shaderInt64 = VK_TRUE;
            f2.features.fillModeNonSolid = VK_TRUE;
            f2.features.samplerAnisotropy = VK_TRUE;
            f2.features.multiDrawIndirect = VK_TRUE;

            auto& f12 = chain.Feature<VkPhysicalDeviceVulkan12Features>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
            f12.drawIndirectCount = VK_TRUE;
            f12.timelineSemaphore = VK_TRUE;
            f12.scalarBlockLayout = VK_TRUE;
            f12.descriptorIndexing = VK_TRUE;
            f12.samplerFilterMinmax = VK_TRUE;
            f12.bufferDeviceAddress = VK_TRUE;
            f12.imagelessFramebuffer = VK_TRUE;
            f12.runtimeDescriptorArray = VK_TRUE;
            f12.descriptorBindingPartiallyBound = VK_TRUE;
            f12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
            f12.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
            f12.shaderInputAttachmentArrayNonUniformIndexing = VK_TRUE;
            f12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
            f12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;

            auto& f13 = chain.Feature<VkPhysicalDeviceVulkan13Features>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
            f13.maintenance4 = VK_TRUE;
            f13.dynamicRendering = VK_TRUE;
            f13.synchronization2 = VK_TRUE;

            chain.Extension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR)
                .fragmentShaderBarycentric = VK_TRUE;

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
        }

        if (config.descriptorBuffers)
        {
            chain.Extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
            auto& db = chain.Feature<VkPhysicalDeviceDescriptorBufferFeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT);
            db.descriptorBuffer = VK_TRUE;
            db.descriptorBufferPushDescriptors = VK_TRUE;
        }

        if (config.rayTracing)
        {
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

        auto deviceExtensions = NOVA_ALLOC_STACK(const char*, chain.extensions.size());
        {
            u32 i = 0;
            for (const auto& ext : chain.extensions)
                deviceExtensions[i++] = ext.c_str();
        }

        VkCall(vkCreateDevice(gpu, Temp(VkDeviceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = chain.Build(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = std::array {
                VkDeviceQueueCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = Get(graphics).family,
                    .queueCount = 1,
                    .pQueuePriorities = Temp(1.f),
                },
            }.data(),
            .enabledExtensionCount = u32(chain.extensions.size()),
            .ppEnabledExtensionNames = deviceExtensions,
        }), pAlloc, &device));

        volkLoadDevice(device);

        vkGetPhysicalDeviceProperties2(gpu, Temp(VkPhysicalDeviceProperties2 {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &descriptorSizes,
        }));

        // ---- Shared resources ----

        vkGetDeviceQueue(device, Get(graphics).family, 0, &Get(graphics).handle);

        VkCall(vmaCreateAllocator(Temp(VmaAllocatorCreateInfo {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = gpu,
            .device = device,
            .pAllocationCallbacks = pAlloc,
            .pVulkanFunctions = Temp(VmaVulkanFunctions {
                .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
            }),
            .instance = instance,
            .vulkanApiVersion = VK_API_VERSION_1_3,
        }), &vma));

        {
            // Already rely on a bottomless descriptor pool, so not going to pretend to pass some bogus sizes here.
            // TODO: Check what platforms have bottomless implementations
            // TODO: Runtime profile-based amortized constant pool reallocation?

            constexpr u32 MaxDescriptorPerType = 65'536;
            constexpr auto sizes = std::array {
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLER,                MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       MaxDescriptorPerType },
            };

            VkCall(vkCreateDescriptorPool(device, Temp(VkDescriptorPoolCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                .maxSets = u32(MaxDescriptorPerType * sizes.size()),
                .poolSizeCount = u32(sizes.size()),
                .pPoolSizes = sizes.data(),
            }), pAlloc, &descriptorPool));
        }

        VkCall(vkCreatePipelineCache(device, Temp(VkPipelineCacheCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .initialDataSize = 0,
        }), pAlloc, &pipelineCache));
    }

    VulkanContext::~VulkanContext()
    {
        WaitIdle();

        // Destroy API objects
        swapchains.ForEach(     [&](auto id, auto&) { Destroy(id); });
        buffers.ForEach(        [&](auto id, auto&) { Destroy(id); });
        fences.ForEach(         [&](auto id, auto&) { Destroy(id); });
        commandPools.ForEach(   [&](auto id, auto&) { Destroy(id); });
        textures.ForEach(       [&](auto id, auto&) { Destroy(id); });
        pipelineLayouts.ForEach([&](auto id, auto&) { Destroy(id); });
        shaders.ForEach(        [&](auto id, auto&) { Destroy(id); });

        // Deleted graphics pipeline library stages
        for (auto&[key, pipeline] : vertexInputStages)    vkDestroyPipeline(device, pipeline, pAlloc);
        for (auto&[key, pipeline] : preRasterStages)      vkDestroyPipeline(device, pipeline, pAlloc);
        for (auto&[key, pipeline] : fragmentShaderStages) vkDestroyPipeline(device, pipeline, pAlloc);
        for (auto&[key, pipeline] : fragmentOutputStages) vkDestroyPipeline(device, pipeline, pAlloc);
        for (auto&[key, pipeline] : graphicsPipelineSets) vkDestroyPipeline(device, pipeline, pAlloc);

        // Destroy context vk objects
        vkDestroyPipelineCache(device, pipelineCache, pAlloc);
        vkDestroyDescriptorPool(device, descriptorPool, pAlloc);
        vmaDestroyAllocator(vma);
        vkDestroyDevice(device, pAlloc);
        if (debugMessenger)
            vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, pAlloc);
        vkDestroyInstance(instance, pAlloc);

        NOVA_LOG("~Context(Allocations = {})", AllocationCount.load());
    }

    void VulkanContext::WaitIdle()
    {
        vkDeviceWaitIdle(device);
    }
}