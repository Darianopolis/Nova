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

    struct VulkanFeature
    {
        VkStructureType sType;
        const char*      name;
        usz              size;
        usz            offset;
    };

#define NOVA_VK_EXTENSION(Name) Name

#define NOVA_VK_FEATURE(Struct, Field) \
    VulkanFeature { vkh::GetStructureType<Struct>(), #Struct "::" #Field, sizeof(Struct), offsetof(Struct, Field) }

    struct VulkanDeviceConfiguration
    {
        using FeatureOrExtension = std::variant<VulkanFeature, const char*>;

        std::vector<const char*>                                extensions;
        HashMap<VkStructureType, VkBaseInStructure*>       device_features;
        VkBaseInStructure*                                            next = nullptr;

        Context          ctx;
        VkPhysicalDevice gpu = nullptr;

        bool trace = true;

        ~VulkanDeviceConfiguration()
        {
            for (const auto& feature: device_features | std::views::values) {
                std::free(feature);
            }
        }

        [[nodiscard]] bool IsSupported(const char* extension) const
        {
            NOVA_STACK_POINT();

            u32 count = 0;
            ctx->vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, nullptr);

            auto* props = NOVA_STACK_ALLOC(VkExtensionProperties, count);
            vkh::Check(ctx->vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, props));

            for (u32 i = 0; i < count; ++i) {
                if (strcmp(extension, props[i].extensionName) == 0) {
                    return true;
                }
            }

            return false;
        }

        bool Add(const char* extension)
        {
            if (IsSupported(extension)) {
                if (!std::ranges::any_of(extensions, [&](auto&& ext) { return strcmp(extension, ext) == 0; })) {
                    extensions.emplace_back(extension);
                }
                if (trace) {
                    NOVA_LOG("Extension added         [{}]", extension);
                }
                return true;
            }
            if (trace) {
                NOVA_LOG("Extension not supported [{}]", extension);
            }
            return false;
        }

        void Require(const char* extension)
        {
            if (!Add(extension)) {
                NOVA_THROW("Required extension [{}] not supported", extension);
            }
        }

        [[nodiscard]] bool IsSupported(const VulkanFeature& feature) const
        {
            NOVA_STACK_POINT();

            auto* requested = NOVA_STACK_ALLOC(std::byte, feature.size);
            std::memset(requested, 0, feature.size);
            reinterpret_cast<VkBaseInStructure*>(requested)->sType = feature.sType;

            ctx->vkGetPhysicalDeviceFeatures2(gpu, Temp(VkPhysicalDeviceFeatures2 {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext = requested,
            }));

            return *reinterpret_cast<VkBool32*>(requested + feature.offset) == VK_TRUE;
        }

        bool Add(const VulkanFeature& feature)
        {
            if (IsSupported(feature)) {
                auto*& target_features = device_features[feature.sType];
                if (!target_features) {
                    target_features = static_cast<VkBaseInStructure*>(std::malloc(feature.size));
                    std::memset(target_features, 0, feature.size);
                    target_features->sType = feature.sType;
                    target_features->pNext = next;
                    next = target_features;
                }
                GetFieldAtByteOffset<VkBool32>(*target_features, feature.offset) = VK_TRUE;
                if (trace) {
                    NOVA_LOG("Feature   added         [{}] ", feature.name);
                }
                return true;
            }
            if (trace) {
                NOVA_LOG("Feature   not supported [{}] ", feature.name);
            }
            return false;
        }

        void Require(const VulkanFeature& feature)
        {
            if (!Add(feature)) {
                NOVA_THROW("Required feature [{}] not supported", feature.name);
            }
        }

        bool AddAll(Span<FeatureOrExtension> items)
        {
            bool all_present = true;
            for (auto& item : items) {
                if (!std::visit([this](auto&& item_) {
                    bool supported = IsSupported(item_);
                    if (!supported) {
                        // This will always fail, we're just using it to get the trace logging message
                        Add(item_);
                    }
                    return supported;
                }, item)) {
                    all_present = false;
                }
            }

            if (all_present) {
                for (auto& item : items) {
                    std::visit([this](auto&& item_) { Add(item_); }, item);
                }
            }

            return all_present;
        }

        [[nodiscard]] const void* Build() const
        {
            return next;
        }
    };

    Context Context::Create(const ContextConfig& config)
    {
        NOVA_STACK_POINT();

        auto context_create_start = std::chrono::steady_clock::now();
        NOVA_DEFER(&) {
            if (!exceptions) {
                auto end = std::chrono::steady_clock::now();
                NOVA_LOG("Vulkan context created in {}", DurationToString(end - context_create_start));
            }
        };

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

        if (gpus.empty()) {
            NOVA_LOG("Critical error: No physical devices found");
        }

        if (auto gpu_override = std::getenv("NOVA_GPU_SELECT")) {
            auto index = std::stoi(gpu_override);
            if (index >= gpus.size()) {
                NOVA_LOG("Invalid index provided for GPU override: {}", index);
            } else {
                VkPhysicalDeviceProperties2 properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
                impl->gpu = gpus[index];
                impl->vkGetPhysicalDeviceProperties2(impl->gpu, &properties);
                NOVA_LOG("Overriding GPU selection [{}]: {}", index, properties.properties.deviceName);
            }
        }

        if (!impl->gpu) {
            NOVA_LOG("Automatically selecting GPU");
            for (auto& gpu : gpus) {
                VkPhysicalDeviceProperties2 properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
                impl->vkGetPhysicalDeviceProperties2(gpu, &properties);
                if (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                    impl->gpu = gpu;
                    NOVA_LOG("  Found discrete GPU: {}", properties.properties.deviceName);
                    break;
                }
            }

            if (!impl->gpu) {
                NOVA_LOG("  No discrete GPU found, defaulting to first available GPU");
                impl->gpu = gpus.front();
            }
        }

        // Configure features

        VulkanDeviceConfiguration chain;
        chain.ctx = { impl };
        chain.gpu = impl->gpu;

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

        // Swapchains

        chain.Require(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        // Core Features

        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceFeatures2, features.imageCubeArray));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceFeatures2, features.independentBlend));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceFeatures2, features.multiDrawIndirect));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceFeatures2, features.drawIndirectFirstInstance));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceFeatures2, features.fillModeNonSolid));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceFeatures2, features.wideLines));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceFeatures2, features.multiViewport));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceFeatures2, features.samplerAnisotropy));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceFeatures2, features.fragmentStoresAndAtomics));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceFeatures2, features.shaderInt64));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceFeatures2, features.shaderInt16));

        // External memory imports (for DXGI interop)

        chain.Add(NOVA_VK_EXTENSION(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME));
        chain.Add(NOVA_VK_EXTENSION("VK_KHR_external_memory_win32"));

        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan11Features, storageBuffer16BitAccess));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan11Features, storagePushConstant16));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan11Features, multiview));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan11Features, multiviewGeometryShader));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan11Features, multiviewTessellationShader));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan11Features, shaderDrawParameters));

        // Vulkan 1.2

        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, drawIndirectCount));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, storageBuffer8BitAccess));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, storagePushConstant8));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, shaderInt8));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, shaderFloat16));
        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorIndexing));
        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, shaderSampledImageArrayNonUniformIndexing));
        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, shaderStorageImageArrayNonUniformIndexing));
        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, shaderInputAttachmentArrayNonUniformIndexing));
        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingSampledImageUpdateAfterBind));
        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingStorageImageUpdateAfterBind));
        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingUpdateUnusedWhilePending));
        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingPartiallyBound));
        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, runtimeDescriptorArray));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, samplerFilterMinmax));
        chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, scalarBlockLayout));
        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, timelineSemaphore));
        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan12Features, bufferDeviceAddress));

        // Vulkan 1.3

        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan13Features, synchronization2));
        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan13Features, dynamicRendering));
        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceVulkan13Features, maintenance4));

        // Extended Dynamic State

        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceExtendedDynamicStateFeaturesEXT, extendedDynamicState));
        chain.Require(NOVA_VK_FEATURE(VkPhysicalDeviceExtendedDynamicState2FeaturesEXT, extendedDynamicState2));

        // Graphics Pipeline Libraries

        if (chain.AddAll({
            NOVA_VK_EXTENSION(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME),
            NOVA_VK_EXTENSION(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME),
            NOVA_VK_FEATURE(VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT, graphicsPipelineLibrary),
        })) {
            impl->graphics_pipeline_library = true;
        }

        // Image transfer staging (host image copy + resizable BAR)

        impl->transfer_manager.staged_image_copy = true;
        if (impl->resizable_bar && chain.AddAll({
            NOVA_VK_EXTENSION(VK_EXT_HOST_IMAGE_COPY_EXTENSION_NAME),
            NOVA_VK_FEATURE(VkPhysicalDeviceHostImageCopyFeaturesEXT, hostImageCopy),
        })) {
            impl->transfer_manager.staged_image_copy = false;
        }

        // Shader objects

        if (auto allow_shader_objects = std::getenv("NOVA_SHADER_OBJECTS");
                allow_shader_objects && allow_shader_objects[0] == '1') {
            if (chain.AddAll({
                NOVA_VK_EXTENSION(VK_EXT_SHADER_OBJECT_EXTENSION_NAME),
                NOVA_VK_FEATURE(VkPhysicalDeviceShaderObjectFeaturesEXT, shaderObject),
            })) {
                impl->shader_objects = true;
            }
        }

        // Descriptor buffers

        if (chain.AddAll({
            NOVA_VK_EXTENSION(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME),
            NOVA_VK_FEATURE(VkPhysicalDeviceDescriptorBufferFeaturesEXT, descriptorBuffer),
            NOVA_VK_FEATURE(VkPhysicalDeviceDescriptorBufferFeaturesEXT, descriptorBufferPushDescriptors),
        })) {
            impl->descriptor_buffers = true;
        }

        // Fragment Shader barycentrics

        chain.AddAll({
            NOVA_VK_EXTENSION(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME),
            NOVA_VK_FEATURE(VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR, fragmentShaderBarycentric)
        });

        // Fragment Shader Interlock

        if (chain.Add(NOVA_VK_EXTENSION(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME))) {
            chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT, fragmentShaderSampleInterlock));
            chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT, fragmentShaderPixelInterlock));
            chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT, fragmentShaderShadingRateInterlock));
        }

        // Mesh shading

        if (chain.AddAll({
            NOVA_VK_EXTENSION(VK_EXT_MESH_SHADER_EXTENSION_NAME),
            NOVA_VK_FEATURE(VkPhysicalDeviceMeshShaderFeaturesEXT, taskShader),
            NOVA_VK_FEATURE(VkPhysicalDeviceMeshShaderFeaturesEXT, meshShader),
        })) {
            chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceMeshShaderFeaturesEXT, multiviewMeshShader));
            chain.Add(NOVA_VK_FEATURE(VkPhysicalDeviceMeshShaderFeaturesEXT, meshShaderQueries));
            impl->mesh_shading = true;
        }

        // Ray Tracing

        if (config.ray_tracing) {
            if (chain.AddAll({
                NOVA_VK_EXTENSION(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME),
                NOVA_VK_EXTENSION(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME),
                NOVA_VK_EXTENSION(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME),
                NOVA_VK_FEATURE(VkPhysicalDeviceRayTracingPipelineFeaturesKHR, rayTracingPipeline),
                NOVA_VK_FEATURE(VkPhysicalDeviceAccelerationStructureFeaturesKHR, accelerationStructure),
            })) {
                chain.AddAll({
                    NOVA_VK_EXTENSION(VK_KHR_RAY_QUERY_EXTENSION_NAME),
                    NOVA_VK_FEATURE(VkPhysicalDeviceRayQueryFeaturesKHR, rayQuery),
                });

                chain.AddAll({
                    NOVA_VK_EXTENSION(VK_NV_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME),
                    NOVA_VK_FEATURE(VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV, rayTracingInvocationReorder),
                });

                chain.AddAll({
                    NOVA_VK_EXTENSION(VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME),
                    NOVA_VK_FEATURE(VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR, rayTracingPositionFetch),
                });
            } else {
                NOVA_THROW("Ray tracing requested, but core ray tracing functionality not available");
            }
        }

        // Configure queues

        struct QueueFamilyInfo
        {
            u32   family_index;
            u32          count;
            VkQueueFlags flags;
        };

        std::optional<QueueFamilyInfo> graphics_family;
        std::optional<QueueFamilyInfo> dedicated_transfer_family;
        std::optional<QueueFamilyInfo> async_compute_family;

        std::vector<VkQueueFamilyProperties> queue_properties;
        {
            u32 count;
            impl->vkGetPhysicalDeviceQueueFamilyProperties(impl->gpu, &count, nullptr);
            queue_properties.resize(count);
            impl->vkGetPhysicalDeviceQueueFamilyProperties(impl->gpu, &count, queue_properties.data());
        }

        // TODO: Handle other weird queue configs

        for (u32 family = 0; family < queue_properties.size(); ++family) {
            auto& props = queue_properties[family];
            if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT
                    && props.queueFlags & VK_QUEUE_COMPUTE_BIT
                    && props.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                if (!graphics_family) {
                    graphics_family = QueueFamilyInfo {
                        .family_index = family,
                        .count = props.queueCount,
                        .flags = props.queueFlags,
                    };
                }
            } else if (props.queueFlags & VK_QUEUE_COMPUTE_BIT
                    && props.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                if (!async_compute_family) {
                    async_compute_family = QueueFamilyInfo {
                        .family_index = family,
                        .count = props.queueCount,
                        .flags = props.queueFlags,
                    };
                }
            } else if (props.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                if (!dedicated_transfer_family) {
                    dedicated_transfer_family = QueueFamilyInfo {
                        .family_index = family,
                        .count = props.queueCount,
                        .flags = props.queueFlags,
                    };
                }
            }
        }

        std::array queue_priorities { 1.f, 1.f };
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

        auto GenerateQueues = [&](QueueFamilyInfo& info, u32 max_count, VkPipelineStageFlags2 stages, std::vector<nova::Queue>& queues) {
            auto count = std::min(info.count, max_count);
            for (u32 i = 0; i < count; ++i) {
                queues.emplace_back(new Queue::Impl {
                    .context = { impl },
                    .flags = info.flags,
                    .family = info.family_index,
                    .stages = stages,
                });
            }

            queue_create_infos.emplace_back(VkDeviceQueueCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = info.family_index,
                .queueCount = count,
                .pQueuePriorities = queue_priorities.data(),
            });

            NOVA_LOG("Creating {} queues with index {}", count, info.family_index);
        };

        if (graphics_family) {
            GenerateQueues(graphics_family.value(), 1,
                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                impl->graphics_queues);
        }

        if (async_compute_family) {
            GenerateQueues(async_compute_family.value(), 2,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                impl->compute_queues);
        }

        if (dedicated_transfer_family) {
            GenerateQueues(dedicated_transfer_family.value(), 1,
                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                impl->transfer_queues);
        }

        // Create device

        vkh::Check(impl->vkCreateDevice(impl->gpu, Temp(VkDeviceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = chain.Build(),
            .queueCreateInfoCount = u32(queue_create_infos.size()),
            .pQueueCreateInfos = queue_create_infos.data(),
            .enabledExtensionCount = u32(chain.extensions.size()),
            .ppEnabledExtensionNames = chain.extensions.data(),
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

        impl->properties.max_multiview_count = impl->vulkan11_properties.maxMultiviewViewCount;

        // Get queues

        auto GetQueues = [&](std::vector<nova::Queue>& queues) {
            for (u32 i = 0; i < queues.size(); ++i) {
                impl->vkGetDeviceQueue(impl->device, queues[i]->family, i, &queues[i]->handle);
            }
        };

        GetQueues(impl->graphics_queues);
        GetQueues(impl->compute_queues);
        GetQueues(impl->transfer_queues);

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
            constexpr u32 MaxNumImageDescriptors   = 1024 * 1024;
            constexpr u32 MaxNumSamplerDescriptors = 4096;
            constexpr u32 MaxPushConstantSize      = 256;

            VkPhysicalDeviceProperties2 props = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
            impl->vkGetPhysicalDeviceProperties2(impl->gpu, &props);

            u32 num_image_descriptors = std::min({
                MaxNumImageDescriptors,
                props.properties.limits.maxDescriptorSetSampledImages,
                props.properties.limits.maxDescriptorSetStorageImages
            });

            impl->properties.max_push_constant_size = std::min(MaxPushConstantSize, props.properties.limits.maxPushConstantsSize);

            u32 num_sampler_descriptors = std::min(MaxNumSamplerDescriptors, props.properties.limits.maxSamplerAllocationCount);

            NOVA_LOG("Heap image descriptors: {}", num_image_descriptors);
            NOVA_LOG("Heap sampler descriptors: {}", num_sampler_descriptors);
            NOVA_LOG("Push constant size: {}", impl->properties.max_push_constant_size);

            impl->global_heap.Init(impl, num_image_descriptors, num_sampler_descriptors);
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

        for (auto& queue : impl->graphics_queues) { delete queue.impl; }
        for (auto& queue : impl->compute_queues)  { delete queue.impl; }
        for (auto& queue : impl->transfer_queues) { delete queue.impl; }

        // Deleted graphics pipeline library stages

        for (auto pipeline: impl->vertex_input_stages | std::views::values)    { impl->vkDestroyPipeline(impl->device, pipeline, impl->alloc); }
        for (auto pipeline: impl->preraster_stages | std::views::values)       { impl->vkDestroyPipeline(impl->device, pipeline, impl->alloc); }
        for (auto pipeline: impl->fragment_shader_stages | std::views::values) { impl->vkDestroyPipeline(impl->device, pipeline, impl->alloc); }
        for (auto pipeline: impl->fragment_output_stages | std::views::values) { impl->vkDestroyPipeline(impl->device, pipeline, impl->alloc); }
        for (auto pipeline: impl->graphics_pipeline_sets | std::views::values) { impl->vkDestroyPipeline(impl->device, pipeline, impl->alloc); }
        for (auto pipeline: impl->compute_pipelines | std::views::values)      { impl->vkDestroyPipeline(impl->device, pipeline, impl->alloc); }

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

    const ContextProperties& Context::GetProperties() const
    {
        return impl->properties;
    }


    void* Vulkan_TrackedAllocate(void*, size_t size, size_t align, VkSystemAllocationScope)
    {
        align = std::max(8ull, align);

        void* ptr = _aligned_offset_malloc(size + 8, align, 8);

        if (ptr) {
            static_cast<usz*>(ptr)[0] = size;

            rhi::stats::MemoryAllocated += size;
            ++rhi::stats::AllocationCount;
            ++rhi::stats::NewAllocationCount;

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
            --rhi::stats::AllocationCount;
        }

        void* ptr = _aligned_offset_realloc(ByteOffsetPointer(orig, -8), size + 8, align, 8);
        static_cast<usz*>(ptr)[0] = size;

        if (ptr) {
            rhi::stats::MemoryAllocated += size;
            ++rhi::stats::AllocationCount;
            if (ptr != orig) {
                ++rhi::stats::NewAllocationCount;
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
            --rhi::stats::AllocationCount;

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
        ++rhi::stats::AllocationCount;
        ++rhi::stats::NewAllocationCount;

    }

    void Vulkan_NotifyFree(void*, size_t size, VkInternalAllocationType, VkSystemAllocationScope)
    {
#ifdef NOVA_RHI_NOISY_ALLOCATIONS
        NOVA_LOG("Internal free of size {}, type = {}", size, int(type));
#endif
        rhi::stats::MemoryAllocated -= size;
        --rhi::stats::AllocationCount;
    }
}