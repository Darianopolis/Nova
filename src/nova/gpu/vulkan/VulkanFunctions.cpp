#include <nova/gpu/vulkan/VulkanRHI.hpp>

#include <concepts>
#include <tuple>

namespace
{
    using namespace nova;

    using VoidFuncPtr = void(*)();

    struct VulkanFunctionName
    {
        size_t        offset;
        const char*     name;
        VoidFuncPtr unloaded;
        bool     is_instance = false;
    };

    template<typename T>
    struct FunctionPtrTraits;

    template<typename R, typename ...Args>
    struct FunctionPtrTraits<R(*)(Args...)>
    {
        static const size_t nargs = sizeof...(Args);

        typedef R result_type;

        template <size_t i>
        struct arg
        {
            using type = std::tuple_element<i, std::tuple<Args...>>::type;
        };
    };

    constexpr VulkanFunctionName VulkanFunctionNames[] {
#define NOVA_VULKAN_FUNCTION(vk_function, ...)                                 \
    VulkanFunctionName{offsetof(Handle<Context>::Impl, vk_function),           \
        #vk_function,                                                          \
        +[] { NOVA_THROW("Attemped to call unloaded vulkan function: " #vk_function); }, \
        __VA_OPT__(__VA_ARGS__ |) !std::same_as<FunctionPtrTraits<PFN_##vk_function>::arg<0>::type, VkDevice> \
    },
#include "VulkanFunctions.inl"
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
            if (func.is_instance) continue;
            auto fptr = vk_functions->vkGetDeviceProcAddr(device, func.name);
            if (fptr) {
                *ByteOffsetPointer((VoidFuncPtr*)vk_functions, func.offset) = fptr;
            }
        }
    }
}
