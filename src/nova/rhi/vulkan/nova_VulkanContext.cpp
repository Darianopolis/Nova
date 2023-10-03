#include "nova_VulkanRHI.hpp"

namespace nova
{
    static
    VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        [[maybe_unused]] void* userData)
    {
        if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                || type != VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            return VK_FALSE;
        }

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

        std::terminate();

        // return VK_FALSE;
    }

    struct VulkanFeatureChain
    {
        HashMap<VkStructureType, VkBaseInStructure*> deviceFeatures;
        ankerl::unordered_dense::set<std::string> extensions;
        VkBaseInStructure* pNext = nullptr;

        ~VulkanFeatureChain()
        {
            for (auto&[type, feature] : deviceFeatures) {
                mi_free(feature);
            }
        }

        void Extension(std::string name)
        {
            extensions.emplace(std::move(name));
        }

        template<typename T>
        T& Feature(VkStructureType type)
        {
            auto& f = deviceFeatures[type];
            if (!f) {
                f = static_cast<VkBaseInStructure*>(mi_malloc(sizeof(T)));
                new(f) T{};
                f->sType = type;
                f->pNext = pNext;
                pNext = f;
            }

            return *(T*)f;
        }

        const void* Build()
        {
            return pNext;
        }
    };

    Context Context::Create(const ContextConfig& config)
    {
        auto impl = new Impl;
        impl->config = config;

        std::vector<const char*> instanceLayers;
        if (config.debug) {
            instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
        }

        // instanceLayers.push_back("VK_LAYER_LUNARG_api_dump");

        std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };
    #ifdef _WIN32
        instanceExtensions.push_back("VK_KHR_win32_surface");
    #endif
        if (config.debug) {
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
            .pUserData = impl,
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

        vkh::Check(vkCreateInstance(Temp(VkInstanceCreateInfo {
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
        }), impl->pAlloc, &impl->instance));

        volkLoadInstanceOnly(impl->instance);

        if (config.debug)
            vkh::Check(vkCreateDebugUtilsMessengerEXT(impl->instance, &debugMessengerCreateInfo, impl->pAlloc, &impl->debugMessenger));


        std::vector<VkPhysicalDevice> gpus;
        vkh::Enumerate(gpus, vkEnumeratePhysicalDevices, impl->instance);
        for (auto& gpu : gpus) {
            VkPhysicalDeviceProperties2 properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
            vkGetPhysicalDeviceProperties2(gpu, &properties);
            if (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                impl->gpu = gpu;

                impl->maxDescriptors = std::min({
                    properties.properties.limits.maxDescriptorSetSampledImages,
                    properties.properties.limits.maxDescriptorSetStorageImages,
                    properties.properties.limits.maxDescriptorSetUniformBuffers,
                    properties.properties.limits.maxDescriptorSetStorageBuffers,
                    properties.properties.limits.maxDescriptorSetSamplers,
                });
                break;
            }
        }
        // u32 gpuCount = 1;
        // vkh::Check(vkEnumeratePhysicalDevices(impl->instance, &gpuCount, &impl->gpu));
        // if (gpuCount == 0)
        //     NOVA_THROW("No physical devices found!");

        // ---- Logical Device ----

        std::array<VkQueueFamilyProperties, 3> properties;
        vkGetPhysicalDeviceQueueFamilyProperties(impl->gpu, Temp(3u), properties.data());
        for (u32 i = 0; i < 16; ++i) {
            auto queue = new Queue::Impl;
            queue->context = { impl };
            queue->stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            queue->family = 0;
            impl->graphicQueues.emplace_back(queue);
        }
        for (u32 i = 0; i < 2; ++i) {
            auto queue = new Queue::Impl;
            queue->context = { impl };
            queue->stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            queue->family = 1;
            impl->transferQueues.emplace_back(queue);
        }
        for (u32 i = 0; i < 8; ++i) {
            auto queue = new Queue::Impl;
            queue->context = { impl };
            queue->stages = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            queue->family = 2;
            impl->computeQueues.emplace_back(queue);
        }

        VulkanFeatureChain chain;

        // TODO: Allow for optional features

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
            }

            {
                auto& f = chain.Feature<VkPhysicalDeviceVulkan11Features>(
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
                f.storagePushConstant16 = VK_TRUE;
                f.storageBuffer16BitAccess = VK_TRUE;
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
                f.runtimeDescriptorArray = VK_TRUE;
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

            // Mesh Shaders

            {
                chain.Extension(VK_EXT_MESH_SHADER_EXTENSION_NAME);
                auto& f = chain.Feature<VkPhysicalDeviceMeshShaderFeaturesEXT>(
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT);
                f.meshShader = true;
                f.meshShaderQueries = true;
                f.multiviewMeshShader = true;
                f.taskShader = true;
            }

            // Mutable Descriptor Type

            chain.Extension(VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT)
                .mutableDescriptorType = VK_TRUE;

            // Host Image Copy

            chain.Extension(VK_EXT_HOST_IMAGE_COPY_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceHostImageCopyFeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT)
                .hostImageCopy = VK_TRUE;

#if 0
            // Shader Objects

            chain.Extension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceShaderObjectFeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT)
                .shaderObject = VK_TRUE;
            impl->shaderObjectsSupported = true;
#endif

            // Descriptor Buffers

            {
                chain.Extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
                auto& f = chain.Feature<VkPhysicalDeviceDescriptorBufferFeaturesEXT>(
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT);
                f.descriptorBuffer = VK_TRUE;
                f.descriptorBufferPushDescriptors = VK_TRUE;
                impl->descriptorBuffers = true;
            }

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

            // Fragment Shader Interlock

            {
                chain.Extension(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME);
                auto& f = chain.Feature<VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT>(
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT);
                f.fragmentShaderPixelInterlock = VK_TRUE;
                f.fragmentShaderSampleInterlock = VK_TRUE;
                f.fragmentShaderShadingRateInterlock = VK_TRUE;
            }
        }

        if (config.rayTracing) {

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

        auto deviceExtensions = NOVA_ALLOC_STACK(const char*, chain.extensions.size());
        {
            u32 i = 0;

            for (const auto& ext : chain.extensions) {
                deviceExtensions[i++] = ext.c_str();
            }
        }

        {
            std::vector<VkExtensionProperties> props;
            vkh::Enumerate(props, vkEnumerateDeviceExtensionProperties, impl->gpu, nullptr);

            std::unordered_set<std::string_view> supported;
            for (auto& prop : props) {
                supported.insert(prop.extensionName);
            }

            u32 missingCount = 0;
            for (auto& ext : chain.extensions) {
                if (!supported.contains(ext)) {
                    missingCount++;
                    NOVA_LOG("Missing extension: {}", ext);
                }
            }

            if (missingCount) {
                NOVA_LOG("Missing {} extension{}.\nPress Enter to close...",
                    missingCount, missingCount > 1 ? "s" : "");
                // Log to file to avoid needing this?
                std::getline(std::cin, *Temp(std::string{}));
                NOVA_THROW("Missing {} extensions.", missingCount);
            }
        }

        auto priorities = NOVA_ALLOC_STACK(float, impl->graphicQueues.size());
        for (u32 i = 0; i < impl->graphicQueues.size(); ++i) {
            priorities[i] = 1.f;
        }

        auto lowPriorities = NOVA_ALLOC_STACK(float, 8);
        for (u32 i = 0; i < 8; ++i) {
            lowPriorities[i] = 0.1f;
        }

        vkh::Check(vkCreateDevice(impl->gpu, Temp(VkDeviceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = chain.Build(),
            .queueCreateInfoCount = 3,
            .pQueueCreateInfos = std::array {
                VkDeviceQueueCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = impl->graphicQueues.front()->family,
                    .queueCount = u32(impl->graphicQueues.size()),
                    .pQueuePriorities = priorities,
                },
                VkDeviceQueueCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = impl->transferQueues.front()->family,
                    .queueCount = u32(impl->transferQueues.size()),
                    .pQueuePriorities = lowPriorities,
                },
                VkDeviceQueueCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = impl->computeQueues.front()->family,
                    .queueCount = u32(impl->computeQueues.size()),
                    .pQueuePriorities = lowPriorities,
                },
            }.data(),
            .enabledExtensionCount = u32(chain.extensions.size()),
            .ppEnabledExtensionNames = deviceExtensions,
        }), impl->pAlloc, &impl->device));

        volkLoadDevice(impl->device);

        vkGetPhysicalDeviceProperties2(impl->gpu, Temp(VkPhysicalDeviceProperties2 {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &impl->descriptorSizes,
        }));

        // ---- Shared resources ----

        for (u32 i = 0; i < impl->graphicQueues.size(); ++i) {
            vkGetDeviceQueue(impl->device, impl->graphicQueues[i]->family, i, &impl->graphicQueues[i]->handle);
        }

        for (u32 i = 0; i < impl->transferQueues.size(); ++i) {
            vkGetDeviceQueue(impl->device, impl->transferQueues[i]->family, i, &impl->transferQueues[i]->handle);
        }

        for (u32 i = 0; i < impl->computeQueues.size(); ++i) {
            vkGetDeviceQueue(impl->device, impl->computeQueues[i]->family, i, &impl->computeQueues[i]->handle);
        }

        vkh::Check(vmaCreateAllocator(Temp(VmaAllocatorCreateInfo {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = impl->gpu,
            .device = impl->device,
            .pAllocationCallbacks = impl->pAlloc,
            .pVulkanFunctions = Temp(VmaVulkanFunctions {
                .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
            }),
            .instance = impl->instance,
            .vulkanApiVersion = VK_API_VERSION_1_3,
        }), &impl->vma));

        vkh::Check(vkCreatePipelineCache(impl->device, Temp(VkPipelineCacheCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .initialDataSize = 0,
        }), impl->pAlloc, &impl->pipelineCache));

        vkh::Check(vkCreateDescriptorSetLayout(impl->device, Temp(VkDescriptorSetLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = Temp(VkDescriptorSetLayoutBindingFlagsCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .pNext = Temp(VkMutableDescriptorTypeCreateInfoEXT {
                    .sType = VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_VALVE,
                    .mutableDescriptorTypeListCount = 1,
                    .pMutableDescriptorTypeLists = Temp(VkMutableDescriptorTypeListEXT {
                        .descriptorTypeCount = 8,
                        .pDescriptorTypes = std::array {
                            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,

                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,

                            VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                            VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,

                            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,

                            VK_DESCRIPTOR_TYPE_SAMPLER,
                        }.data(),
                    }),
                }),
                .bindingCount = 1,
                .pBindingFlags = std::array<VkDescriptorBindingFlags, 1> {
                    VkDescriptorSetLayoutCreateFlags(impl->descriptorBuffers
                            ? 0 : VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT)
                        | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                        | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
                }.data(),
            }),
            .flags = VkDescriptorSetLayoutCreateFlags(impl->descriptorBuffers
                ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                : VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT),
            .bindingCount = 1,
            .pBindings = std::array {
                VkDescriptorSetLayoutBinding {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_MUTABLE_EXT,
                    .descriptorCount = impl->maxDescriptors,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                },
            }.data(),
        }), impl->pAlloc, &impl->heapLayout));

        if (impl->descriptorBuffers) {
            VkDeviceSize maxSize;
            vkGetDescriptorSetLayoutSizeEXT(impl->device, impl->heapLayout, &maxSize);
            NOVA_LOGEXPR(maxSize);
            impl->mutableDescriptorSize = u32(maxSize / impl->maxDescriptors);
            NOVA_LOGEXPR(impl->mutableDescriptorSize);
        }

        vkh::Check(vkCreatePipelineLayout(impl->device, Temp(VkPipelineLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &impl->heapLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = Temp(VkPushConstantRange {
                .stageFlags = VK_SHADER_STAGE_ALL,
                .size = 256,
            }),
        }), impl->pAlloc, &impl->pipelineLayout));

        return { impl };
    }

    void Context::Destroy()
    {
        if (!impl) {
            return;
        }

        WaitIdle();

        for (auto& queue : impl->graphicQueues)  { delete queue.impl; }
        for (auto& queue : impl->computeQueues)  { delete queue.impl; }
        for (auto& queue : impl->transferQueues) { delete queue.impl; }

        // Deleted graphics pipeline library stages
        for (auto&[key, pipeline] : impl->vertexInputStages)    { vkDestroyPipeline(impl->device, pipeline, impl->pAlloc); }
        for (auto&[key, pipeline] : impl->preRasterStages)      { vkDestroyPipeline(impl->device, pipeline, impl->pAlloc); }
        for (auto&[key, pipeline] : impl->fragmentShaderStages) { vkDestroyPipeline(impl->device, pipeline, impl->pAlloc); }
        for (auto&[key, pipeline] : impl->fragmentOutputStages) { vkDestroyPipeline(impl->device, pipeline, impl->pAlloc); }
        for (auto&[key, pipeline] : impl->graphicsPipelineSets) { vkDestroyPipeline(impl->device, pipeline, impl->pAlloc); }
        for (auto&[key, pipeline] : impl->computePipelines)     { vkDestroyPipeline(impl->device, pipeline, impl->pAlloc); }

        // Destroy context vk objects
        vkDestroyPipelineLayout(impl->device, impl->pipelineLayout, impl->pAlloc);
        vkDestroyDescriptorSetLayout(impl->device, impl->heapLayout, impl->pAlloc);
        vkDestroyPipelineCache(impl->device, impl->pipelineCache, impl->pAlloc);
        vmaDestroyAllocator(impl->vma);
        vkDestroyDevice(impl->device, impl->pAlloc);
        if (impl->debugMessenger) {
            vkDestroyDebugUtilsMessengerEXT(impl->instance, impl->debugMessenger, impl->pAlloc);
        }
        vkDestroyInstance(impl->instance, impl->pAlloc);

        NOVA_LOG("~Context(Allocations = {})", rhi::stats::AllocationCount.load());

        delete impl;
        impl = nullptr;
    }

    void Context::WaitIdle() const
    {
        vkDeviceWaitIdle(impl->device);
    }

    const ContextConfig& Context::GetConfig() const
    {
        return impl->config;
    }
}