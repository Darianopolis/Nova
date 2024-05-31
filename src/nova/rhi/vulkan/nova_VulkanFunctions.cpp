#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

namespace
{
    using namespace nova;

    using VoidFuncPtr = void(*)();

    struct VulkanFunctionName
    {
        size_t        offset;
        const char*     name;
        VoidFuncPtr unloaded;
    };

    constexpr VulkanFunctionName VulkanFunctionNames[] {
#define NOVA_VULKAN_FUNCTION(vk_function) VulkanFunctionName(offsetof(Handle<Context>::Impl, vk_function), #vk_function, +[] { NOVA_THROW("Attemped to call unloaded vulkan function: " #vk_function); }),
#include "nova_VulkanFunctions.inl"
#undef NOVA_VULKAN_FUNCTION
    };
}

namespace nova
{
    void VulkanFunctions_Init(Handle<Context>::Impl* vk_functions, PFN_vkGetInstanceProcAddr loader)
    {
        for (auto& func : VulkanFunctionNames) {
            auto fptr = loader(nullptr, func.name);
            if (fptr) {
                *ByteOffsetPointer((VoidFuncPtr*)vk_functions, func.offset) = fptr;
            } else {
                *ByteOffsetPointer((VoidFuncPtr*)vk_functions, func.offset) = func.unloaded;
            }
        }
        vk_functions->vkGetInstanceProcAddr = loader;
    }

    void VulkanFunctions_LoadInstance(Handle<Context>::Impl* vk_functions, VkInstance instance)
    {
        for (auto& func : VulkanFunctionNames) {
            auto fptr = vk_functions->vkGetInstanceProcAddr(instance, func.name);
            if (fptr) {
                *ByteOffsetPointer((VoidFuncPtr*)vk_functions, func.offset) = fptr;
            }
        }
    }

    void VulkanFunctions_LoadDevice(Handle<Context>::Impl* vk_functions, VkDevice device)
    {
        for (auto& func : VulkanFunctionNames) {
            auto fptr = vk_functions->vkGetDeviceProcAddr(device, func.name);
            if (fptr) {
                *ByteOffsetPointer((VoidFuncPtr*)vk_functions, func.offset) = fptr;
            }
        }
    }
}