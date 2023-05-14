#include "VulkanBackend.hpp"

namespace pyr
{
    Ref<Context> CreateContext(b8 debug)
    {
        Ref ctx = new Context;

        std::vector<const char*> instanceLayers;
        if (debug)
            instanceLayers.push_back("VK_LAYER_KHRONOS_validation");

        std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };
    #ifdef _WIN32
        instanceExtensions.push_back("VK_KHR_win32_surface");
    #endif

        volkInitialize();

        VkCall(vkCreateInstance(Temp(VkInstanceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = Temp(VkApplicationInfo {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .apiVersion = VK_API_VERSION_1_3,
            }),
            .enabledLayerCount = u32(instanceLayers.size()),
            .ppEnabledLayerNames = instanceLayers.data(),
            .enabledExtensionCount = u32(instanceExtensions.size()),
            .ppEnabledExtensionNames = instanceExtensions.data(),
        }), nullptr, &ctx->instance));

        volkLoadInstanceOnly(ctx->instance);

        std::vector<VkPhysicalDevice> gpus;
        PYR_VKQUERY(gpus, vkEnumeratePhysicalDevices, ctx->instance);
        for (auto& _gpu : gpus)
        {
            VkPhysicalDeviceProperties2 properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
            vkGetPhysicalDeviceProperties2(_gpu, &properties);
            if (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                ctx->gpu = _gpu;
                break;
            }
        }

        // ---- Logical Device ----

        vkGetPhysicalDeviceQueueFamilyProperties2(ctx->gpu, Temp(0u), nullptr);
        ctx->graphics.family = 0;

        VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR rtPosFetchFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR };
        rtPosFetchFeatures.rayTracingPositionFetch = VK_TRUE;

        VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR barycentricFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR };
        barycentricFeatures.pNext = &rtPosFetchFeatures;
        barycentricFeatures.fragmentShaderBarycentric = VK_TRUE;

        VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV rtInvocationReorderFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV };
        rtInvocationReorderFeatures.pNext = &barycentricFeatures;
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

        VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT };
        descriptorBufferFeatures.pNext = &rtPipelineFeatures;
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

        auto deviceExtensions = std::array {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
            VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,

            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_NV_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME,

            VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME,

            VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME,
        };

        VkCall(vkCreateDevice(ctx->gpu, Temp(VkDeviceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &features2,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = std::array {
                VkDeviceQueueCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = ctx->graphics.family,
                    .queueCount = 1,
                    .pQueuePriorities = Temp(1.f),
                },
            }.data(),
            .enabledExtensionCount = u32(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
        }), nullptr, &ctx->device));

        volkLoadDevice(ctx->device);

        vkGetPhysicalDeviceProperties2(ctx->gpu, Temp(VkPhysicalDeviceProperties2 {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &ctx->descriptorSizes,
        }));

        // ---- Shared resources ----

        vkGetDeviceQueue(ctx->device, ctx->graphics.family, 0, &ctx->graphics.handle);

        VkCall(vmaCreateAllocator(Temp(VmaAllocatorCreateInfo {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = ctx->gpu,
            .device = ctx->device,
            .pVulkanFunctions = Temp(VmaVulkanFunctions {
                .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
            }),
            .instance = ctx->instance,
            .vulkanApiVersion = VK_API_VERSION_1_3,
        }), &ctx->vma));

        VkCall(vkCreateFence(ctx->device, Temp(VkFenceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        }), nullptr, &ctx->fence));

        ctx->staging = ctx->CreateBuffer(256ull * 1024 * 1024, 0, BufferFlags::CreateMapped);

        ctx->transferCommands = ctx->CreateCommands();
        ctx->transferCmd = ctx->transferCommands->Allocate();

        ctx->commands = ctx->CreateCommands();
        ctx->cmd = ctx->commands->Allocate();

        ctx->semaphore = ctx->MakeSemaphore();

        // auto createCommands = [&](VkCommandBuffer& cmd, VkCommandPool& pool) {
        //     VkCall(vkCreateCommandPool(ctx->device, Temp(VkCommandPoolCreateInfo {
        //         .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        //         .queueFamilyIndex = ctx->queueFamily,
        //     }), nullptr, &pool));

        //     VkCall(vkAllocateCommandBuffers(ctx->device, Temp(VkCommandBufferAllocateInfo {
        //         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        //         .commandPool = pool,
        //         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        //         .commandBufferCount = 1
        //     }), &cmd));

        //     VkCall(vkBeginCommandBuffer(cmd, Temp(VkCommandBufferBeginInfo {
        //         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        //     })));
        // };

        // createCommands(ctx->cmd, ctx->pool);
        // createCommands(ctx->transferCmd, ctx->transferPool);

        return ctx;
    }

    Context::~Context()
    {
        PYR_LOG("Destroying vulkan context");
    }
}