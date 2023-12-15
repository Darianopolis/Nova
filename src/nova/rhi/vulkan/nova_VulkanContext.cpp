#include <nova/core/nova_Guards.hpp>

#include "nova_VulkanRHI.hpp"

#include "nova_VulkanStructureTypes.hpp"

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

    struct VulkanFeatureStructBase
    {
        VkStructureType sType;
        void*           pNext;
        VkBool32     features[100];
    };

    struct VulkanFeatureStructRef
    {
        VulkanFeatureStructBase* base;
        const std::type_info*    type;
        usz                      size;
        usz                     count;
    };

    struct VulkanFeatureSet
    {
        std::vector<const char*>          extensions;
        std::vector<VulkanFeatureStructRef> features;

        void Extension(const char* name)
        {
            extensions.push_back(name);
        }

        template<class T>
        T& Feature()
        {
            auto* mem = std::malloc(sizeof(T));
            std::memset(mem, 0, sizeof(T));
            auto* base = static_cast<VulkanFeatureStructBase*>(mem);
            base->sType = vkh::GetStructureType<T>();
            features.emplace_back(VulkanFeatureStructRef {
                .base = base,
                .type = &typeid(T),
                .size = sizeof(T),
                .count = (sizeof(T) - offsetof(VulkanFeatureStructBase, features)) / sizeof(VkBool32),
            });
            return *static_cast<T*>(mem);
        }

        bool IsSupported(Context ctx, VkPhysicalDevice gpu)
        {
            // Check extensions

            if (!extensions.empty()) {
                std::vector<VkExtensionProperties> props;
                vkh::Enumerate(props, ctx->vkEnumerateDeviceExtensionProperties, gpu, nullptr);

                std::unordered_set<std::string_view> supported;
                for (auto& prop : props) {
                    supported.insert(prop.extensionName);
                }

                for (auto& ext : extensions) {
                    if (!supported.contains(std::string_view(ext))) {
                        NOVA_LOG("Extension [{}] not supported", ext);
                        return false;
                    }
                }
            }

            // Check features

            for (auto[features_requested, id, size, count] : features) {
                VkPhysicalDeviceFeatures2 features2 {
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                };
                NOVA_STACK_POINT();
                auto* features_present = reinterpret_cast<VulkanFeatureStructBase*>(NOVA_STACK_ALLOC(std::byte, size));
                std::memset(features_present, 0, size);
                features_present->sType = features_requested->sType;
                features2.pNext = features_present;

                ctx->vkGetPhysicalDeviceFeatures2(gpu, &features2);

                for (usz i = 0; i < count; ++i) {
                    if (features_requested->features[i] && !features_present->features[i]) {
                        NOVA_LOG("Feature[{}][{}] not supported", id->name(), i);
                        NOVA_LOG("  sType: {}", int(features_present->sType));
                        NOVA_LOG("  size = {}, count = {}", size, count);
                        return false;
                    }
                }
            }

            return true;
        }

        ~VulkanFeatureSet()
        {
            for (auto& f : features) {
                std::free(f.base);
            }
        }
    };

    struct VulkanFeatureChain
    {

        struct FeatureStructFactory
        {
            VkStructureType     structure_type;
            usz                           size = 0;
            const char*                   name = nullptr;
            HashMap<usz, const char*> features;
        };


        // HashMap<std::type_index, FeatureStructFactory> device_feature_factories;
        HashMap<std::type_index, VulkanFeatureStructBase*> device_features;
        ankerl::unordered_dense::set<std::string>               extensions;
        VulkanFeatureStructBase*                                      next = nullptr;

        ~VulkanFeatureChain()
        {
            for (auto&[type, feature] : device_features) {
                std::free(feature);
            }
        }

        // void Extension(std::string name)
        // {
        //     extensions.emplace(std::move(name));
        // }
        //
        // template<typename T>
        // T& Feature()
        // {
        //     auto& f = device_features[std::type_index(typeid(T))];
        //     if (!f) {
        //         f = static_cast<VkBaseInStructure*>(std::malloc(sizeof(T)));
        //         new(f) T{};
        //         f->sType = vkh::GetStructureType<T>();
        //         f->pNext = next;
        //         next = f;
        //     }
        //
        //     {
        //         auto& factory = device_feature_factories[std::type_index(typeid(T))];
        //         if (!factory.name) {
        //             factory.structure_type = vkh::GetStructureType<T>();
        //             factory.size = sizeof(T);
        //             factory.name = typeid(T).name();
        //         }
        //     }
        //
        //     return *(T*)f;
        // }

        void AddFeatureSet(const VulkanFeatureSet& set)
        {
            for (auto& ext : set.extensions) {
                extensions.emplace(ext);
            }

            for (const auto& [requested_features, id, size, count] : set.features) {
                auto*& target_features = device_features[std::type_index(*id)];
                if (!target_features) {
                    target_features = static_cast<VulkanFeatureStructBase*>(std::malloc(size));
                    std::memcpy(target_features, requested_features, size);
                    target_features->sType = requested_features->sType;
                    target_features->pNext = next;
                    next = target_features;
                }

                // Add requested features
                for (usz i = 0; i < count; ++i) {
                    target_features->features[i] |= requested_features->features[i];
                }
            }
        }

        const void* Build() const
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
        Platform_AddPlatformExtensions(instance_extensions);

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

            VulkanFeatureSet core;

            core.Extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

            // Core Features

            core.Feature<VkPhysicalDeviceFeatures2>().features.imageCubeArray = true;
            core.Feature<VkPhysicalDeviceFeatures2>().features.independentBlend = true;
            core.Feature<VkPhysicalDeviceFeatures2>().features.multiDrawIndirect = true;
            core.Feature<VkPhysicalDeviceFeatures2>().features.drawIndirectFirstInstance = true;
            core.Feature<VkPhysicalDeviceFeatures2>().features.fillModeNonSolid = true;
            core.Feature<VkPhysicalDeviceFeatures2>().features.wideLines = true;
            core.Feature<VkPhysicalDeviceFeatures2>().features.multiViewport = true;
            core.Feature<VkPhysicalDeviceFeatures2>().features.samplerAnisotropy = true;
            core.Feature<VkPhysicalDeviceFeatures2>().features.fragmentStoresAndAtomics = true;
            core.Feature<VkPhysicalDeviceFeatures2>().features.shaderInt64 = true;
            core.Feature<VkPhysicalDeviceFeatures2>().features.shaderInt16 = true;

            // External memory imports (for DXGI interop)

            core.Extension(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
#ifdef NOVA_PLATFORM_WINDOWS
            core.Extension("VK_KHR_external_memory_win32");
#endif

            core.Feature<VkPhysicalDeviceVulkan11Features>().storageBuffer16BitAccess = true;
            core.Feature<VkPhysicalDeviceVulkan11Features>().storagePushConstant16 = true;
            core.Feature<VkPhysicalDeviceVulkan11Features>().multiview = true;
            core.Feature<VkPhysicalDeviceVulkan11Features>().multiviewGeometryShader = true;
            core.Feature<VkPhysicalDeviceVulkan11Features>().multiviewTessellationShader = true;
            core.Feature<VkPhysicalDeviceVulkan11Features>().shaderDrawParameters = true;

            // Vulkan 1.2

            core.Feature<VkPhysicalDeviceVulkan12Features>().drawIndirectCount = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().storageBuffer8BitAccess = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().storagePushConstant8 = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().shaderInt8 = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().descriptorIndexing = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().shaderSampledImageArrayNonUniformIndexing = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().shaderStorageBufferArrayNonUniformIndexing = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().shaderStorageImageArrayNonUniformIndexing = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().shaderInputAttachmentArrayNonUniformIndexing = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().descriptorBindingSampledImageUpdateAfterBind = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().descriptorBindingStorageImageUpdateAfterBind = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().descriptorBindingUpdateUnusedWhilePending = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().descriptorBindingPartiallyBound = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().descriptorBindingVariableDescriptorCount = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().runtimeDescriptorArray = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().samplerFilterMinmax = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().scalarBlockLayout = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().imagelessFramebuffer = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().timelineSemaphore = true;
            core.Feature<VkPhysicalDeviceVulkan12Features>().bufferDeviceAddress = true;

            // Vulkan 1.3

            core.Feature<VkPhysicalDeviceVulkan13Features>().synchronization2 = true;
            core.Feature<VkPhysicalDeviceVulkan13Features>().dynamicRendering = true;
            core.Feature<VkPhysicalDeviceVulkan13Features>().maintenance4 = true;

            // Graphics Pipeline Libraries + Extended Dynamic State

            core.Feature<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState = true;
            core.Feature<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT>().extendedDynamicState2 = true;
            core.Extension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
            core.Extension(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
            core.Feature<VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT>().graphicsPipelineLibrary = true;

            if (!core.IsSupported({impl}, impl->gpu)) {
                NOVA_THROW("Core functionality not supported on target device");
            }

            chain.AddFeatureSet(core);
        }

        {
            // Check for resizable BAR
            VkPhysicalDeviceMemoryProperties props = {};
            impl->vkGetPhysicalDeviceMemoryProperties(impl->gpu, &props);

            usz max_device_memory = 0;
            for (u32 i = 0; i < props.memoryHeapCount; ++i) {
                auto& heap = props.memoryHeaps[i];
                if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                    max_device_memory = std::max(max_device_memory, heap.size);
                }
            }

            usz max_host_visible_device_memory = 0;
            for (u32 i = 0; i < props.memoryTypeCount; ++i) {
                auto& type = props.memoryTypes[i];
                if ((type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                        && (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                    max_host_visible_device_memory = std::max(max_host_visible_device_memory, props.memoryHeaps[type.heapIndex].size);
                }
            }

            if (max_device_memory == max_host_visible_device_memory) {
                impl->resizable_bar = true;
            }
        }

        if (impl->resizable_bar) {

            // Host Image Copy

            VulkanFeatureSet image_copy;
            image_copy.Extension(VK_EXT_HOST_IMAGE_COPY_EXTENSION_NAME);
            image_copy.Feature<VkPhysicalDeviceHostImageCopyFeaturesEXT>().hostImageCopy = true;
            if (image_copy.IsSupported({impl}, impl->gpu)) {
                chain.AddFeatureSet(image_copy);
            } else {
                NOVA_LOG("Warn: Resizable BAR enabled, but image host copy not found - falling back to staged image transfers");
            }
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

        {
            // Fragment Shader Barycentrics

            VulkanFeatureSet fragment_shader_bary;
            fragment_shader_bary.Extension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
            fragment_shader_bary.Feature<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR>().fragmentShaderBarycentric = true;

            if (fragment_shader_bary.IsSupported({impl}, impl->gpu)) {
                chain.AddFeatureSet(fragment_shader_bary);
            }
        }

        {
            // Fragment Shader Interlock

            VulkanFeatureSet fragment_shader_interlock;
            fragment_shader_interlock.Extension(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME);
            fragment_shader_interlock.Feature<VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT>().fragmentShaderSampleInterlock = true;
            fragment_shader_interlock.Feature<VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT>().fragmentShaderPixelInterlock = true;
            fragment_shader_interlock.Feature<VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT>().fragmentShaderShadingRateInterlock = true;

            if (fragment_shader_interlock.IsSupported({impl}, impl->gpu)) {
                chain.AddFeatureSet(fragment_shader_interlock);
            }
        }

        if (config.mesh_shading) {
            // Mesh Shading extensions

            VulkanFeatureSet set;
            set.Extension(VK_EXT_MESH_SHADER_EXTENSION_NAME);
            set.Feature<VkPhysicalDeviceMeshShaderFeaturesEXT>().taskShader = true;
            set.Feature<VkPhysicalDeviceMeshShaderFeaturesEXT>().meshShader = true;
            set.Feature<VkPhysicalDeviceMeshShaderFeaturesEXT>().multiviewMeshShader = true;
            set.Feature<VkPhysicalDeviceMeshShaderFeaturesEXT>().meshShaderQueries = true;

            if (set.IsSupported({impl}, impl->gpu)) {
                chain.AddFeatureSet(set);
            } else {
                NOVA_THROW("Mesh shading requested, but is not supported on target device");
            }
        }

        if (config.ray_tracing) {

            // Ray Tracing extensions

            VulkanFeatureSet rt_core;

            rt_core.Extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
            rt_core.Extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
            rt_core.Extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            rt_core.Feature<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>().rayTracingPipeline = true;
            rt_core.Feature<VkPhysicalDeviceAccelerationStructureFeaturesKHR>().accelerationStructure = true;
            if (rt_core.IsSupported({impl}, impl->gpu)) {
                chain.AddFeatureSet(rt_core);
            } else {
                NOVA_THROW("Ray tracing requested, but core RT extensions/features not supported on device");
            }

            VulkanFeatureSet rt_query;
            rt_query.Extension(VK_KHR_RAY_QUERY_EXTENSION_NAME);
            rt_query.Feature<VkPhysicalDeviceRayQueryFeaturesKHR>().rayQuery = true;
            if (rt_query.IsSupported({impl}, impl->gpu)) {
                chain.AddFeatureSet(rt_query);
            } else {
                NOVA_LOG("Warn: RT Query not supported");
            }

            VulkanFeatureSet rt_reorder;
            rt_reorder.Extension(VK_NV_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME);
            rt_reorder.Feature<VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV>().rayTracingInvocationReorder = true;
            if (rt_reorder.IsSupported({impl}, impl->gpu)) {
                chain.AddFeatureSet(rt_reorder);
            } else {
                NOVA_LOG("Warn: RT Invocation Reorder not supported");
            }

            VulkanFeatureSet rt_posfetch;
            rt_posfetch.Extension(VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME);
            rt_posfetch.Feature<VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR>().rayTracingPositionFetch = true;
            if (rt_posfetch.IsSupported({impl}, impl->gpu)) {
                chain.AddFeatureSet(rt_posfetch);
            } else {
                NOVA_LOG("Warn: RT Position Fetch not supported");
            }
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
            impl->graphics_queues.emplace_back(queue);
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

        auto priorities = NOVA_STACK_ALLOC(float, impl->graphics_queues.size());
        for (u32 i = 0; i < impl->graphics_queues.size(); ++i) {
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
                    .queueFamilyIndex = impl->graphics_queues.front()->family,
                    .queueCount = u32(impl->graphics_queues.size()),
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

        for (u32 i = 0; i < impl->graphics_queues.size(); ++i) {
            impl->vkGetDeviceQueue(impl->device, impl->graphics_queues[i]->family, i, &impl->graphics_queues[i]->handle);
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

        for (auto& queue : impl->graphics_queues)  { delete queue.impl; }
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

    void* Vulkan_TrackedAllocate(void*, size_t size, size_t align, VkSystemAllocationScope)
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

    void* Vulkan_TrackedReallocate(void*, void* orig, size_t size, size_t align, VkSystemAllocationScope)
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

    void Vulkan_TrackedFree(void*, void* ptr)
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

    void Vulkan_NotifyAllocation(void*, size_t size, VkInternalAllocationType, VkSystemAllocationScope)
    {
#ifdef NOVA_RHI_NOISY_ALLOCATIONS
        NOVA_LOG("Internal allocation of size {}, type = {}", size, int(type));
#endif
        rhi::stats::MemoryAllocated += size;
        rhi::stats::AllocationCount++;
        rhi::stats::NewAllocationCount++;

    }

    void Vulkan_NotifyFree(void*, size_t size, VkInternalAllocationType, VkSystemAllocationScope)
    {
#ifdef NOVA_RHI_NOISY_ALLOCATIONS
        NOVA_LOG("Internal free of size {}, type = {}", size, int(type));
#endif
        rhi::stats::MemoryAllocated -= size;
        rhi::stats::AllocationCount--;
    }
}