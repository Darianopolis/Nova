#pragma once

#include "nova_RHI.hpp"

// -----------------------------------------------------------------------------

#define NOVA_DECL_DEVICE_PROC(name)  inline PFN_##name name
#define NOVA_LOAD_DEVICE_PROC(name, device) ::nova::name = (PFN_##name)vkGetDeviceProcAddr(device, #name);\
    NOVA_LOG("Loaded fn [" #name "] - {}", (void*)name)

// -----------------------------------------------------------------------------

namespace nova
{
    struct BufferImpl : ImplBase
    {
        ContextImpl* context = {};

        VkBuffer          buffer = {};
        VmaAllocation allocation = {};
        VkDeviceSize        size = 0ull;
        VkDeviceAddress  address = 0ull;
        b8*               mapped = nullptr;
        BufferFlags        flags = BufferFlags::None;
        VkBufferUsageFlags usage = {};

    public:
        ~BufferImpl();
    };

// -----------------------------------------------------------------------------

    struct SamplerImpl : ImplBase
    {
        ContextImpl* context = {};

        VkSampler sampler = {};

    public:
        ~SamplerImpl();
    };

// -----------------------------------------------------------------------------

    struct TextureImpl : ImplBase
    {
        ContextImpl* context = {};

        VkImage             image = {};
        VmaAllocation  allocation = {};
        VkImageView          view = {};
        VkImageUsageFlags   usage = {};
        VkFormat           format = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

        Vec3U extent = {};
        u32     mips = 0;
        u32   layers = 0;

    public:
        ~TextureImpl();
    };

// -----------------------------------------------------------------------------

    struct ShaderImpl : ImplBase
    {
        ContextImpl* context = {};

        VkShaderStageFlagBits stage = VkShaderStageFlagBits(0);
        VkShaderEXT          shader = {};
        VkShaderModule       module = {};

        constexpr static const char* EntryPoint = "main";

    public:
        ~ShaderImpl();
    };

// -----------------------------------------------------------------------------

    struct SurfaceImpl : ImplBase
    {
        ContextImpl* context = {};

        VkSurfaceKHR surface = {};
    public:
        ~SurfaceImpl();
    };

// -----------------------------------------------------------------------------

    struct SwapchainImpl : ImplBase
    {
        ContextImpl* context = {};

        VkSurfaceKHR           surface = nullptr;
        VkSwapchainKHR       swapchain = nullptr;
        VkSurfaceFormatKHR      format = { VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        VkImageUsageFlags        usage = 0;
        VkPresentModeKHR   presentMode = VK_PRESENT_MODE_FIFO_KHR;
        std::vector<Texture>  textures = {};
        uint32_t                 index = UINT32_MAX;
        VkExtent2D              extent = { 0, 0 };
        bool                   invalid = false;

        std::vector<VkSemaphore> semaphores = {};
        u32                  semaphoreIndex = 0;

    public:
        ~SwapchainImpl();
    };

// -----------------------------------------------------------------------------

    struct QueueImpl : ImplBase
    {
        ContextImpl* context = {};

        VkQueue handle = {};
        u32     family = UINT32_MAX;
    };

// -----------------------------------------------------------------------------

    struct FenceImpl : ImplBase
    {
        ContextImpl* context = {};

        VkSemaphore semaphore = {};
        u64             value = 0;

    public:
        ~FenceImpl();
    };

// -----------------------------------------------------------------------------

    struct DescriptorSetLayoutImpl : ImplBase
    {
        ContextImpl* context = {};

        VkDescriptorSetLayout layout = {};
        u64                     size = 0;
        std::vector<u64>     offsets = {};

    public:
        ~DescriptorSetLayoutImpl();
    };

// -----------------------------------------------------------------------------

    struct PipelineLayoutImpl : ImplBase
    {
        ContextImpl* context = {};

        VkPipelineLayout layout = {};

        // TODO: Pipeline layout used in multiple bind points?
        BindPoint bindPoint = {};

        std::vector<VkPushConstantRange> ranges;
        std::vector<VkDescriptorSetLayout> sets;

    public:
        ~PipelineLayoutImpl();
    };

// -----------------------------------------------------------------------------

    struct ResourceTrackerImpl : ImplBase
    {
        struct ImageState
        {
            VkImageLayout        layout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2       access = 0;

            u64 version = 0;
        };

    public:
        ContextImpl* context = {};

        u64                                                   version = 0;
        ankerl::unordered_dense::map<VkImage, ImageState> imageStates;
        std::vector<VkImage>                                clearList;

    public:
        ImageState& Get(Texture texture) noexcept;
    };

// -----------------------------------------------------------------------------

    struct AccelerationStructureBuilderImpl : ImplBase
    {
        ContextImpl* context = {};

        VkAccelerationStructureTypeKHR        type = {};
        VkBuildAccelerationStructureFlagsKHR flags = {};

        u64         buildSize = 0;
        u64  buildScratchSize = 0;
        u64 updateScratchSize = 0;

        std::vector<VkAccelerationStructureGeometryKHR>   geometries;
        std::vector<u32>                             primitiveCounts;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;

        u32 geometryCount = 0;
        u32 firstGeometry = 0;
        bool    sizeDirty = false;

        VkQueryPool queryPool = {};

    public:
        ~AccelerationStructureBuilderImpl();

        void EnsureGeometries(u32 geometryIndex);
        void EnsureSizes();
    };

// -----------------------------------------------------------------------------

    struct AccelerationStructureImpl : ImplBase
    {
        ContextImpl* context = {};

        VkAccelerationStructureKHR structure = {};
        u64                          address = {};
        VkAccelerationStructureTypeKHR  type = {};

        Buffer buffer = {};

    public:
        ~AccelerationStructureImpl();
    };

// -----------------------------------------------------------------------------

    struct RayTracingPipelineImpl : ImplBase
    {
        ContextImpl* context = {};

        VkPipeline pipeline = {};
        Buffer    sbtBuffer = {};

        VkStridedDeviceAddressRegionKHR rayGenRegion = {};
        VkStridedDeviceAddressRegionKHR rayMissRegion = {};
        VkStridedDeviceAddressRegionKHR rayHitRegion = {};
        VkStridedDeviceAddressRegionKHR rayCallRegion = {};

    public:
        ~RayTracingPipelineImpl();
    };

// -----------------------------------------------------------------------------

    struct CommandPoolImpl : ImplBase
    {
        ContextImpl* context = {};
        QueueImpl*     queue = {};

        VkCommandPool               pool = {};
        std::vector<CommandList> buffers = {};
        u32                        index = 0;

        std::vector<CommandList> secondaryBuffers = {};
        u32                        secondaryIndex = 0;

    public:
        ~CommandPoolImpl();
    };

    struct CommandListImpl : ImplBase
    {
        CommandPoolImpl*        pool = {};
        ResourceTrackerImpl* tracker = {};
        VkCommandBuffer       buffer = {};
    };

// -----------------------------------------------------------------------------

    struct ContextImpl : ImplBase
    {
        VkInstance  instance = {};
        VkPhysicalDevice gpu = {};
        VkDevice      device = {};
        VmaAllocator     vma = {};

        VkDebugUtilsMessengerEXT debugMessenger = {};

        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT* data,
            void* userData);

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        };

        VkPhysicalDeviceAccelerationStructurePropertiesKHR accelStructureProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
            .pNext = &rayTracingPipelineProperties,
        };
        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorSizes = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
            .pNext = &accelStructureProperties,
        };

        Queue graphics = {};

        static std::atomic_int64_t    AllocationCount;
        static std::atomic_int64_t NewAllocationCount;

        VkAllocationCallbacks alloc = {
            .pfnAllocation = +[](void*, size_t size, size_t align, [[maybe_unused]] VkSystemAllocationScope scope) {
                void* ptr = mi_malloc_aligned(size, align);
                if (ptr)
                {
                    ++AllocationCount;
                    ++NewAllocationCount;
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                    NOVA_LOG(" --\n{}", std::stacktrace::current());
                    NOVA_LOG("Allocating size = {}, align = {}, scope = {}, ptr = {}", size, align, int(scope), ptr);
#endif
                }
                return ptr;
            },
            .pfnReallocation = +[](void*, void* orig, size_t size, size_t align, VkSystemAllocationScope) {
                void* ptr = mi_realloc_aligned(orig, size, align);
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                NOVA_LOG("Reallocated, size = {}, align = {}, ptr = {} -> {}", size, align, orig, ptr);
#endif
                return ptr;
            },
            .pfnFree = +[](void*, void* ptr) {
                if (ptr)
                {
                    --AllocationCount;
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                    NOVA_LOG("Freeing ptr = {}", ptr);
                    NOVA_LOG("    Allocations - :: {}", AllocationCount.load());
#endif
                }
                mi_free(ptr);
            },
            .pfnInternalAllocation = +[](void*, size_t size, VkInternalAllocationType type, VkSystemAllocationScope) {
                NOVA_LOG("Internal allocation of size {}, type = {}", size, int(type));
            },
            .pfnInternalFree = +[](void*, size_t size, VkInternalAllocationType type, VkSystemAllocationScope) {
                NOVA_LOG("Internal free of size {}, type = {}", size, int(type));
            },
        };
        VkAllocationCallbacks* pAlloc = &alloc;

    public:
        ~ContextImpl();
    };
}
