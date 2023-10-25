#pragma once

#include <nova/rhi/nova_RHI.hpp>

#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace nova
{
    namespace vkh
    {
        inline
        void Check(VkResult res)
        {
            if (res != VK_SUCCESS) {
                NOVA_THROW("Error: {}", int(res));
            }
        }

        template<typename Container, typename Fn, typename ... Args>
        void Enumerate(Container&& container, Fn&& fn, Args&& ... args)
        {
            u32 count;
            fn(std::forward<Args>(args)..., &count, nullptr);
            container.resize(count);
            fn(std::forward<Args>(args)..., &count, container.data());
        }
    }

// -----------------------------------------------------------------------------

    enum class UID : u64 {
        Invalid = 0,
    };

    template<>
    struct Handle<Queue>::Impl
    {
        Context context = {};

        VkQueue               handle = {};
        u32                   family = UINT32_MAX;
        VkPipelineStageFlags2 stages = {};
    };

    template<>
    struct Handle<Fence>::Impl
    {
        Context context = {};

        VkSemaphore semaphore = {};
        u64             value = 0;
    };

    template<>
    struct Handle<CommandPool>::Impl
    {
        Context context = {};

        Queue queue = {};

        VkCommandPool             pool = {};
        std::vector<CommandList> lists = {};
        u32                      index = 0;
    };

    template<>
    struct Handle<CommandList>::Impl
    {
        Context        context = {};
        CommandPool       pool = {};
        VkCommandBuffer buffer = {};

        // Graphics Pipeline Library fallback

        std::vector<Format> colorAttachmentsFormats;
        Format                depthAttachmentFormat = nova::Format::Undefined;
        Format              stencilAttachmentFormat = nova::Format::Undefined;

        PolygonMode polygonMode;
        Topology       topology;

        std::bitset<8>   blendStates;
        std::vector<HShader> shaders;

        bool usingShaderObjects = false;
        bool graphicsStateDirty = false;

        void EnsureGraphicsState();
        void Transition(HTexture, VkImageLayout, VkPipelineStageFlags2);
    };

    template<>
    struct Handle<Swapchain>::Impl
    {
        Context context = {};

        VkSurfaceKHR          surface = {};
        VkSwapchainKHR      swapchain = {};
        VkSurfaceFormatKHR     format = { VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        TextureUsage            usage = {};
        PresentMode       presentMode = PresentMode::Fifo;
        std::vector<Texture> textures = {};
        uint32_t                index = UINT32_MAX;
        VkExtent2D             extent = { 0, 0 };
        bool                  invalid = false;

        std::vector<VkSemaphore> semaphores = {};
        u32                  semaphoreIndex = 0;
    };

    template<>
    struct Handle<DescriptorHeap>::Impl
    {
        Context context = {};

        VkDescriptorPool descriptorPool = {};
        VkDescriptorSet   descriptorSet = {};
        Buffer         descriptorBuffer = {};
        u32             descriptorCount = 0;
    };

    template<>
    struct Handle<Shader>::Impl
    {
        Context context = {};

        UID id = UID::Invalid;

        VkShaderModule handle = {};
        std::string     entry = {};

        VkShaderEXT    shader = {};
        ShaderStage     stage = {};

    public:
        VkPipelineShaderStageCreateInfo GetStageInfo() const;
    };

    template<>
    struct Handle<Buffer>::Impl
    {
        Context context = {};

        VkBuffer          buffer = {};
        VmaAllocation allocation = {};
        VkDeviceSize        size = 0ull;
        VkDeviceAddress  address = 0ull;
        BufferFlags        flags = BufferFlags::None;
        BufferUsage        usage = {};
    };

    template<>
    struct Handle<Sampler>::Impl
    {
        Context context = {};

        VkSampler sampler;
    };

    template<>
    struct Handle<Texture>::Impl
    {
        Context context = {};

        VkImage             image = {};
        VmaAllocation  allocation = {};
        VkImageView          view = {};
        TextureUsage        usage = {};
        Format             format = Format::Undefined;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

        VkImageLayout        layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;

        Vec3U extent = {};
        u32     mips = 0;
        u32   layers = 0;
    };

    template<>
    struct Handle<AccelerationStructureBuilder>::Impl
    {
        Context context = {};

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
    };

    template<>
    struct Handle<AccelerationStructure>::Impl
    {
        Context context = {};

        VkAccelerationStructureKHR structure = {};
        u64                          address = {};
        VkAccelerationStructureTypeKHR  type = {};

        Buffer buffer;
        bool ownBuffer;
    };

    template<>
    struct Handle<RayTracingPipeline>::Impl
    {
        Context context = {};

        VkPipeline pipeline = {};
        Buffer    sbtBuffer = {};

        u32                 handleSize;
        u32               handleStride;
        std::vector<u8>        handles;

        VkStridedDeviceAddressRegionKHR  rayGenRegion = {};
        VkStridedDeviceAddressRegionKHR rayMissRegion = {};
        VkStridedDeviceAddressRegionKHR  rayHitRegion = {};
        VkStridedDeviceAddressRegionKHR rayCallRegion = {};
    };

// -----------------------------------------------------------------------------

#define NOVA_DEFINE_WYHASH_EQUALITY(type)                    \
    bool operator==(const type& other) const                 \
    {                                                        \
        return std::memcmp(this, &other, sizeof(type)) == 0; \
    }

    struct GraphicsPipelineVertexInputStageKey
    {
        Topology topology;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelineVertexInputStageKey)
    };

    struct GraphicsPipelinePreRasterizationStageKey
    {
        std::array<UID, 4> shaders;
        PolygonMode       polyMode;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelinePreRasterizationStageKey)
    };

    struct GraphicsPipelineFragmentShaderStageKey
    {
        UID shader;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelineFragmentShaderStageKey)
    };

    struct GraphicsPipelineFragmentOutputStageKey
    {
        std::array<Format, 8> colorAttachments;
        Format                 depthAttachment;
        Format               stencilAttachment;

        std::bitset<8> blendStates;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelineFragmentOutputStageKey)
    };

    struct GraphicsPipelineLibrarySetKey
    {
        std::array<VkPipeline, 4> stages;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelineLibrarySetKey)
    };

    struct ComputePipelineKey
    {
        UID shader;

        NOVA_DEFINE_WYHASH_EQUALITY(ComputePipelineKey)
    };
}

#define NOVA_DEFINE_WYHASH_FOR(type)                          \
    template<> struct ankerl::unordered_dense::hash<type> {   \
        using is_avalanching = void;                          \
        uint64_t operator()(const type& key) const noexcept { \
            return detail::wyhash::hash(&key, sizeof(key));   \
        }                                                     \
    }

NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelineVertexInputStageKey);
NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelinePreRasterizationStageKey);
NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelineFragmentShaderStageKey);
NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelineFragmentOutputStageKey);
NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelineLibrarySetKey);
NOVA_DEFINE_WYHASH_FOR(nova::ComputePipelineKey);

namespace nova
{

// -----------------------------------------------------------------------------

    VkBufferUsageFlags GetVulkanBufferUsage(BufferUsage usage);
    VkImageUsageFlags GetVulkanImageUsage(TextureUsage usage);
    VkFormat GetVulkanFormat(Format format);
    Format FromVulkanFormat(VkFormat format);
    VkIndexType GetVulkanIndexType(IndexType type);
    VkFilter GetVulkanFilter(Filter filter);
    VkSamplerAddressMode GetVulkanAddressMode(AddressMode mode);
    VkBorderColor GetVulkanBorderColor(BorderColor color);
    VkShaderStageFlags GetVulkanShaderStage(ShaderStage in);
    VkPresentModeKHR GetVulkanPresentMode(PresentMode mode);
    VkPipelineBindPoint GetVulkanPipelineBindPoint(BindPoint point);
    VkAccelerationStructureTypeKHR GetVulkanAccelStructureType(AccelerationStructureType type);
    VkBuildAccelerationStructureFlagsKHR GetVulkanAccelStructureBuildFlags(AccelerationStructureFlags in);
    VkDescriptorType GetVulkanDescriptorType(DescriptorType type);
    VkCompareOp GetVulkanCompareOp(CompareOp op);
    VkCullModeFlags GetVulkanCullMode(CullMode in);
    VkFrontFace GetVulkanFrontFace(FrontFace face);
    VkPolygonMode GetVulkanPolygonMode(PolygonMode mode);
    VkPrimitiveTopology GetVulkanTopology(Topology topology);
    VkQueueFlags GetVulkanQueueFlags(QueueFlags in);
    VkPipelineStageFlags2 GetVulkanPipelineStage(PipelineStage in);
    VkImageLayout GetVulkanImageLayout(TextureLayout layout);

// -----------------------------------------------------------------------------
//                                 Context
// -----------------------------------------------------------------------------

    template<>
    struct Handle<Context>::Impl
    {
#define NOVA_VULKAN_FUNCTION(name) PFN_##name name = (PFN_##name)+[]{ NOVA_THROW("Function " #name " not loaded"); };
#include "nova_VulkanFunctions.inl"

        ContextConfig config = {};

        VkInstance  instance = {};
        VkPhysicalDevice gpu = {};
        VkDevice      device = {};
        VmaAllocator     vma = {};

        VkDebugUtilsMessengerEXT debugMessenger = {};

        VkDescriptorSetLayout heapLayout = {};
        VkPipelineLayout  pipelineLayout = {};

        std::vector<Queue>  graphicQueues = {};
        std::vector<Queue> transferQueues = {};
        std::vector<Queue>  computeQueues = {};

// -----------------------------------------------------------------------------
//                              Device Properties
// -----------------------------------------------------------------------------

        bool        shaderObjects = false;
        bool    descriptorBuffers = false;
        u32        maxDescriptors = 0;
        u32 mutableDescriptorSize = 0;

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

// -----------------------------------------------------------------------------
//                             Pipeline caches
// -----------------------------------------------------------------------------

        std::atomic_uint64_t nextUID = 1;
        UID GetUID() noexcept { return UID(nextUID++); };

        HashMap<GraphicsPipelineVertexInputStageKey, VkPipeline>       vertexInputStages;
        HashMap<GraphicsPipelinePreRasterizationStageKey, VkPipeline>    preRasterStages;
        HashMap<GraphicsPipelineFragmentShaderStageKey, VkPipeline> fragmentShaderStages;
        HashMap<GraphicsPipelineFragmentOutputStageKey, VkPipeline> fragmentOutputStages;
        HashMap<GraphicsPipelineLibrarySetKey, VkPipeline>          graphicsPipelineSets;
        HashMap<ComputePipelineKey, VkPipeline>                         computePipelines;
        VkPipelineCache pipelineCache = {};

// -----------------------------------------------------------------------------
//                           Allocation Tracking
// -----------------------------------------------------------------------------

        VkAllocationCallbacks alloc = {
            .pfnAllocation = +[](void*, size_t size, size_t align, VkSystemAllocationScope) {
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
            },
            .pfnReallocation = +[](void*, void* orig, size_t size, size_t align, VkSystemAllocationScope) -> void* {
                align = std::max(8ull, align);

                if (orig) {
                    usz oldSize  = static_cast<usz*>(orig)[-1];

#ifdef NOVA_RHI_NOISY_ALLOCATIONS
                    NOVA_LOG("Reallocating {}, size = {}/{}", orig, oldSize, size);
#endif

                    rhi::stats::MemoryAllocated -= oldSize;
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
            },
            .pfnFree = +[](void*, void* ptr) {
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
            },
            .pfnInternalAllocation = +[](void*, size_t size, VkInternalAllocationType type, VkSystemAllocationScope) {
                NOVA_LOG("Internal allocation of size {}, type = {}", size, int(type));
            },
            .pfnInternalFree = +[](void*, size_t size, VkInternalAllocationType type, VkSystemAllocationScope) {
                NOVA_LOG("Internal free of size {}, type = {}", size, int(type));
            },
        };
        VkAllocationCallbacks* pAlloc = &alloc;
    };
}