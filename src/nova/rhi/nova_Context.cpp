#include "nova_RHI.hpp"

namespace nova
{
    std::atomic_int64_t Context::AllocationCount = 0;
    std::atomic_int64_t Context::NewAllocationCount = 0;

    Context* Context::Create(const ContextConfig& config)
    {
        bool debug = config.debug;

        auto context = new Context;
        NOVA_ON_SCOPE_FAILURE(&) { Destroy(context); };

        std::vector<const char*> instanceLayers;
        if (debug)
            instanceLayers.push_back("VK_LAYER_KHRONOS_validation");

        std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };
    #ifdef _WIN32
        instanceExtensions.push_back("VK_KHR_win32_surface");
    #endif
        if (debug)
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

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
            .pUserData = context,
        };

        VkCall(vkCreateInstance(Temp(VkInstanceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = debug ? &debugMessengerCreateInfo : nullptr,
            .pApplicationInfo = Temp(VkApplicationInfo {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .apiVersion = VK_API_VERSION_1_3,
            }),
            .enabledLayerCount = u32(instanceLayers.size()),
            .ppEnabledLayerNames = instanceLayers.data(),
            .enabledExtensionCount = u32(instanceExtensions.size()),
            .ppEnabledExtensionNames = instanceExtensions.data(),
        }), context->pAlloc, &context->instance));

        volkLoadInstanceOnly(context->instance);

        if (debug)
            VkCall(vkCreateDebugUtilsMessengerEXT(context->instance, &debugMessengerCreateInfo, context->pAlloc, &context->debugMessenger));

        std::vector<VkPhysicalDevice> gpus;
        VkQuery(gpus, vkEnumeratePhysicalDevices, context->instance);
        for (auto& _gpu : gpus)
        {
            VkPhysicalDeviceProperties2 properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
            vkGetPhysicalDeviceProperties2(_gpu, &properties);
            if (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                context->gpu = _gpu;
                break;
            }
        }

        // ---- Logical Device ----

        vkGetPhysicalDeviceQueueFamilyProperties2(context->gpu, Temp(0u), nullptr);
        context->graphics = new Queue;
        context->graphics->context = context;
        context->graphics->family = 0;

        // Ray tracing features

        VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR rtPosFetchFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR };
        rtPosFetchFeatures.rayTracingPositionFetch = VK_TRUE;

        VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV rtInvocationReorderFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV };
        rtInvocationReorderFeatures.pNext = &rtPosFetchFeatures;
        rtInvocationReorderFeatures.rayTracingInvocationReorder = VK_TRUE;

        VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
        rayQueryFeatures.pNext = &rtInvocationReorderFeatures;
        rayQueryFeatures.rayQuery = VK_TRUE;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
        asFeatures.pNext = &rayQueryFeatures;
        asFeatures.accelerationStructure = VK_TRUE;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
        rtPipelineFeatures.pNext = &asFeatures;
        rtPipelineFeatures.rayTracingPipeline = VK_TRUE;

        // Standard features

        VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR barycentricFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR };
        barycentricFeatures.fragmentShaderBarycentric = VK_TRUE;

        VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT };
        descriptorBufferFeatures.pNext = &barycentricFeatures;
        descriptorBufferFeatures.descriptorBuffer = VK_TRUE;
        descriptorBufferFeatures.descriptorBufferPushDescriptors = VK_TRUE;

        VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT };
        shaderObjectFeatures.pNext = &descriptorBufferFeatures;
        shaderObjectFeatures.shaderObject = VK_TRUE;

        VkPhysicalDeviceVulkan12Features vk12Features { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        vk12Features.pNext = &shaderObjectFeatures;
        vk12Features.bufferDeviceAddress = VK_TRUE;
        vk12Features.descriptorIndexing = VK_TRUE;
        vk12Features.runtimeDescriptorArray = VK_TRUE;
        vk12Features.descriptorBindingPartiallyBound = VK_TRUE;
        vk12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        vk12Features.drawIndirectCount = VK_TRUE;
        vk12Features.samplerFilterMinmax = VK_TRUE;
        vk12Features.timelineSemaphore = VK_TRUE;

        VkPhysicalDeviceVulkan13Features vk13Features { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        vk13Features.pNext = &vk12Features;
        vk13Features.dynamicRendering = VK_TRUE;
        vk13Features.synchronization2 = VK_TRUE;
        vk13Features.maintenance4 = VK_TRUE;

        VkPhysicalDeviceFeatures2 features2 { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        features2.pNext = &vk13Features;
        features2.features.shaderInt64 = VK_TRUE;
        features2.features.samplerAnisotropy = VK_TRUE;
        features2.features.wideLines = VK_TRUE;
        features2.features.multiDrawIndirect = VK_TRUE;
        features2.features.fillModeNonSolid = VK_TRUE;

        auto deviceExtensions = std::vector {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,

            VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
            VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,

            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,

            VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME,
        };

        if (config.rayTracing)
        {
            barycentricFeatures.pNext = &rtPipelineFeatures;

            deviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
            deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            deviceExtensions.push_back(VK_NV_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME);
            deviceExtensions.push_back(VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME);
        }

        VkCall(vkCreateDevice(context->gpu, Temp(VkDeviceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &features2,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = std::array {
                VkDeviceQueueCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = context->graphics->family,
                    .queueCount = 1,
                    .pQueuePriorities = Temp(1.f),
                },
            }.data(),
            .enabledExtensionCount = u32(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
        }), context->pAlloc, &context->device));

        volkLoadDevice(context->device);

        vkGetPhysicalDeviceProperties2(context->gpu, Temp(VkPhysicalDeviceProperties2 {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &context->descriptorSizes,
        }));

        // ---- Shared resources ----

        vkGetDeviceQueue(context->device, context->graphics->family, 0, &context->graphics->handle);

        VkCall(vmaCreateAllocator(Temp(VmaAllocatorCreateInfo {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = context->gpu,
            .device = context->device,
            .pAllocationCallbacks = context->pAlloc,
            .pVulkanFunctions = Temp(VmaVulkanFunctions {
                .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
            }),
            .instance = context->instance,
            .vulkanApiVersion = VK_API_VERSION_1_3,
        }), &context->vma));

        return context;
    }

    void Context::Destroy(Context* context)
    {
        delete context->graphics;

        vmaDestroyAllocator(context->vma);
        vkDestroyDevice(context->device, context->pAlloc);
        if (context->debugMessenger)
            vkDestroyDebugUtilsMessengerEXT(context->instance, context->debugMessenger, context->pAlloc);
        vkDestroyInstance(context->instance, context->pAlloc);

        delete context;

        NOVA_LOG("~Context(Allocations = {})", AllocationCount.load());
    }

    VkBool32 VKAPI_CALL Context::DebugCallback(
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

    void Context::WaitForIdle()
    {
        vkDeviceWaitIdle(device);
    }
}