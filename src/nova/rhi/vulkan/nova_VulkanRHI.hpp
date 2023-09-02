#pragma once

#include <nova/rhi/nova_RHI.hpp>

#include <nova/core/nova_Array.hpp>

namespace nova
{
    inline
    void VkCall(VkResult res)
    {
        if (res != VK_SUCCESS) {
            NOVA_THROW("Error: {}", int(res));
        }
    }

    template<typename Container, typename Fn, typename ... Args>
    void VkQuery(Container&& container, Fn&& fn, Args&& ... args)
    {
        u32 count;
        fn(std::forward<Args>(args)..., &count, nullptr);
        container.resize(count);
        fn(std::forward<Args>(args)..., &count, container.data());
    }

// -----------------------------------------------------------------------------

    enum class UID : u64 {
        Invalid = 0,
    };

    struct Queue::Impl
    {
        Context context = {};

        VkQueue               handle = {};
        u32                   family = UINT32_MAX;
        VkPipelineStageFlags2 stages = {};
    };

    struct Fence::Impl
    {
        Context context = {};

        VkSemaphore semaphore = {};
        u64             value = 0;
    };

    struct CommandPool::Impl
    {
        Context context = {};

        Queue queue = {};

        VkCommandPool             pool = {};
        std::vector<CommandList> lists = {};
        u32                      index = 0;
    };

    struct CommandList::Impl
    {
        CommandPool       pool = {};
        VkCommandBuffer buffer = {};

        std::vector<Format> colorAttachmentsFormats;
        Format                depthAttachmentFormat = nova::Format::Undefined;
        Format              stencilAttachmentFormat = nova::Format::Undefined;
        Vec2U                       renderingExtent = {};
    };

    struct Swapchain::Impl
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

    struct DescriptorHeap::Impl
    {
        Context context = {};

        VkDescriptorPool descriptorPool = {};
        VkDescriptorSet   descriptorSet = {};
        u32             descriptorCount = 0;
    };

    struct Shader::Impl
    {
        Context context = {};

        UID id = UID::Invalid;

        VkShaderModule handle = {};
        ShaderStage     stage = {};

    public:
        VkPipelineShaderStageCreateInfo GetStageInfo() const;
    };

    struct Buffer::Impl
    {
        Context context = {};

        VkBuffer          buffer = {};
        VmaAllocation allocation = {};
        VkDeviceSize        size = 0ull;
        VkDeviceAddress  address = 0ull;
        BufferFlags        flags = BufferFlags::None;
        BufferUsage        usage = {};
    };

    struct Sampler::Impl
    {
        Context context = {};

        VkSampler sampler;
    };

    struct Texture::Impl
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

    struct AccelerationStructureBuilder::Impl
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

    struct AccelerationStructure::Impl
    {
        Context context = {};

        VkAccelerationStructureKHR structure = {};
        u64                          address = {};
        VkAccelerationStructureTypeKHR  type = {};

        Buffer buffer;
        bool ownBuffer;
    };

    struct RayTracingPipeline::Impl
    {
        Context context = {};

        VkPipeline pipeline = {};
        Buffer   sbtBuffer = {};

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

#define NOVA_DEFINE_WYHASH_FOR(type)                          \
    template<> struct ankerl::unordered_dense::hash<type> {   \
        using is_avalanching = void;                          \
        uint64_t operator()(const type& key) const noexcept { \
            return detail::wyhash::hash(&key, sizeof(key));   \
        }                                                     \
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

        VkBool32 blendEnable;

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

    struct Context::Impl
    {
        ContextConfig config = {};

        VkInstance  instance = {};
        VkPhysicalDevice gpu = {};
        VkDevice      device = {};
        VmaAllocator     vma = {};

        VkDebugUtilsMessengerEXT debugMessenger = {};

    public:
        u32                  maxDescriptors = 0;
        VkDescriptorSetLayout    heapLayout = {};
        VkDescriptorSetLayout      rtLayout = {};
        VkPipelineLayout     pipelineLayout = {};

        std::vector<Queue>  graphicQueues = {};
        std::vector<Queue> transferQueues = {};
        std::vector<Queue>  computeQueues = {};

    public:
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

    public:
        std::atomic_uint64_t nextUID = 1;
        UID GetUID() noexcept { return UID(nextUID++); };

        ankerl::unordered_dense::map<GraphicsPipelineVertexInputStageKey, VkPipeline>       vertexInputStages;
        ankerl::unordered_dense::map<GraphicsPipelinePreRasterizationStageKey, VkPipeline>    preRasterStages;
        ankerl::unordered_dense::map<GraphicsPipelineFragmentShaderStageKey, VkPipeline> fragmentShaderStages;
        ankerl::unordered_dense::map<GraphicsPipelineFragmentOutputStageKey, VkPipeline> fragmentOutputStages;
        ankerl::unordered_dense::map<GraphicsPipelineLibrarySetKey, VkPipeline>          graphicsPipelineSets;

        ankerl::unordered_dense::map<ComputePipelineKey, VkPipeline> computePipelines;

        VkPipelineCache pipelineCache = {};

    public:
        VkAllocationCallbacks alloc = {
            .pfnAllocation = +[](void*, size_t size, size_t align, [[maybe_unused]] VkSystemAllocationScope scope) {
                void* ptr = mi_malloc_aligned(size, align);
                if (ptr) {
                    rhi::stats::MemoryAllocated += mi_usable_size(ptr);
                    ++rhi::stats::AllocationCount;
                    ++rhi::stats::NewAllocationCount;
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                    std::cout << " --\n" << std::stacktrace::current() << '\n';
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
                if (ptr) {
                    rhi::stats::MemoryAllocated -= mi_usable_size(ptr);
                    --rhi::stats::AllocationCount;
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                    NOVA_LOG("Freeing ptr = {}", ptr);
                    NOVA_LOG("    Allocations - :: {}", rhi::stats::AllocationCount.load());
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
    };
}