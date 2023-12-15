#include <vulkan/vulkan_core.h>

#include "nova_VulkanRHI.hpp"

namespace nova::vkh
{
    template<class VulkanType>
    struct STypeHelper;

    template<class VulkanType>
    consteval VkStructureType GetStructureType()
    {
        return STypeHelper<VulkanType>::type;
    }

    template<> struct STypeHelper<VkPhysicalDeviceFeatures2>                             { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;                                 };
    template<> struct STypeHelper<VkPhysicalDeviceVulkan11Features>                      { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;                        };
    template<> struct STypeHelper<VkPhysicalDeviceVulkan12Features>                      { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;                        };
    template<> struct STypeHelper<VkPhysicalDeviceVulkan13Features>                      { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;                        };
    template<> struct STypeHelper<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>       { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;        };
    template<> struct STypeHelper<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT>      { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT;      };
    template<> struct STypeHelper<VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT>    { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;     };
    template<> struct STypeHelper<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR>  { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR;   };
    template<> struct STypeHelper<VkPhysicalDeviceRayQueryFeaturesKHR>                   { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;                     };
    template<> struct STypeHelper<VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV> { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV; };
    template<> struct STypeHelper<VkPhysicalDeviceHostImageCopyFeaturesEXT>              { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT;               };
    template<> struct STypeHelper<VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT>    { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT;     };
    template<> struct STypeHelper<VkPhysicalDeviceMeshShaderFeaturesEXT>                 { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;                   };
    template<> struct STypeHelper<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>         { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;          };
    template<> struct STypeHelper<VkPhysicalDeviceAccelerationStructureFeaturesKHR>      { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;        };
    template<> struct STypeHelper<VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR>    { constexpr static auto type = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR;    };
}