#pragma once

#include <nova/rhi/nova_RHI.hpp>

#include <nova/core/nova_SubAllocation.hpp>

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

    struct VulkanFormat
    {
        Format     format;
        VkFormat vkFormat;

        u32    atomSize = 0;
        u32  blockWidth = 1;
        u32 blockHeight = 1;
    };

    enum class UID : u64 {
        Invalid = 0,
    };

    template<>
    struct Handle<Queue>::Impl
    {
        Context context = {};

        VkQueueFlags           flags = {};
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

    struct DescriptorHeap
    {
        Context context = {};

        VkDescriptorSetLayout heapLayout = {};
        VkPipelineLayout  pipelineLayout = {};

        VkDescriptorPool  descriptorPool = {};
        VkDescriptorSet    descriptorSet = {};

        Buffer          descriptorBuffer = {};

        u32         imageDescriptorCount = 0;
        u32       samplerDescriptorCount = 0;

        u64 sampledOffset, sampledStride;
        u64 storageOffset, storageStride;
        u64 samplerOffset, samplerStride;

        IndexFreeList   imageHandles;
        IndexFreeList samplerHandles;

        std::shared_mutex mutex;

        void Init(HContext context, u32 imageDescriptorCount, u32 samplerDescriptorCount);
        void Destroy();

        void Bind(CommandList cmd, BindPoint bindPoint);

        void WriteStorage(u32 index, HTexture texture);
        void WriteSampled(u32 index, HTexture texture);
        void WriteSampler(u32 index, HSampler sampler);
    };

    struct TransferManager
    {
        Context context = {};

        Queue         queue = {};
        Fence         fence = {};
        CommandPool cmdPool = {};
        Buffer      staging = {};

        std::shared_mutex mutex;

        bool stagedImageCopy = true;

        void Init(HContext context);
        void Destroy();
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

        u32 descriptorIndex;
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

        u32 descriptorIndex = UINT_MAX;

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

    struct GraphicsPipelineVertexInputStageKey
    {
        Topology topology;

        NOVA_MEMORY_EQUALITY_MEMBER(GraphicsPipelineVertexInputStageKey)
    };

    struct GraphicsPipelinePreRasterizationStageKey
    {
        std::array<UID, 4> shaders;
        PolygonMode       polyMode;

        NOVA_MEMORY_EQUALITY_MEMBER(GraphicsPipelinePreRasterizationStageKey)
    };

    struct GraphicsPipelineFragmentShaderStageKey
    {
        UID shader;

        NOVA_MEMORY_EQUALITY_MEMBER(GraphicsPipelineFragmentShaderStageKey)
    };

    struct GraphicsPipelineFragmentOutputStageKey
    {
        std::array<Format, 8> colorAttachments;
        Format                 depthAttachment;
        Format               stencilAttachment;

        std::bitset<8> blendStates;

        NOVA_MEMORY_EQUALITY_MEMBER(GraphicsPipelineFragmentOutputStageKey)
    };

    struct GraphicsPipelineLibrarySetKey
    {
        std::array<VkPipeline, 4> stages;

        NOVA_MEMORY_EQUALITY_MEMBER(GraphicsPipelineLibrarySetKey)
    };

    struct ComputePipelineKey
    {
        UID shader;

        NOVA_MEMORY_EQUALITY_MEMBER(ComputePipelineKey)
    };
}

NOVA_MEMORY_HASH(nova::GraphicsPipelineVertexInputStageKey);
NOVA_MEMORY_HASH(nova::GraphicsPipelinePreRasterizationStageKey);
NOVA_MEMORY_HASH(nova::GraphicsPipelineFragmentShaderStageKey);
NOVA_MEMORY_HASH(nova::GraphicsPipelineFragmentOutputStageKey);
NOVA_MEMORY_HASH(nova::GraphicsPipelineLibrarySetKey);
NOVA_MEMORY_HASH(nova::ComputePipelineKey);

namespace nova
{

// -----------------------------------------------------------------------------

    VkBufferUsageFlags GetVulkanBufferUsage(BufferUsage usage);
    VkImageUsageFlags GetVulkanImageUsage(TextureUsage usage);
    const VulkanFormat& GetVulkanFormat(Format format);
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

    void* VulkanTrackedAllocate(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
    void* VulkanTrackedReallocate(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
    void  VulkanTrackedFree(void* pUserData, void* pMemory);
    void  VulkanNotifyAllocation(void* pUserData, size_t size, VkInternalAllocationType, VkSystemAllocationScope);
    void  VulkanNotifyFree(void* pUserData, size_t size, VkInternalAllocationType, VkSystemAllocationScope);

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

        DescriptorHeap       globalHeap;
        TransferManager transferManager;

        std::vector<Queue>  graphicQueues = {};
        std::vector<Queue> transferQueues = {};
        std::vector<Queue>  computeQueues = {};

// -----------------------------------------------------------------------------
//                              Device Properties
// -----------------------------------------------------------------------------

        bool        shaderObjects = false;
        bool    descriptorBuffers = false;

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
            .pfnAllocation = VulkanTrackedAllocate,
            .pfnReallocation = VulkanTrackedReallocate,
            .pfnFree = VulkanTrackedFree,
            .pfnInternalAllocation = VulkanNotifyAllocation,
            .pfnInternalFree = VulkanNotifyFree,
        };
        VkAllocationCallbacks* pAlloc = &alloc;
    };
}