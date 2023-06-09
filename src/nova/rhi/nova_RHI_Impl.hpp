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
        Context context = {};

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
        Context context = {};

        VkSampler sampler = {};

    public:
        ~SamplerImpl();
    };

// -----------------------------------------------------------------------------

    struct TextureImpl : ImplBase
    {
        Context context = {};

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
        Context       context = {};
        PipelineLayout layout = {};

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
        Context context = {};

        VkSurfaceKHR surface = {};

    public:
        ~SurfaceImpl();
    };

// -----------------------------------------------------------------------------

    struct SwapchainImpl : ImplBase
    {
        Context context = {};
        Surface surface = {};

        VkSwapchainKHR           swapchain = nullptr;
        VkSurfaceFormatKHR          format = { VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        VkImageUsageFlags            usage = 0;
        VkPresentModeKHR       presentMode = VK_PRESENT_MODE_FIFO_KHR;
        std::vector<Texture::Arc> textures = {};
        uint32_t                     index = UINT32_MAX;
        VkExtent2D                  extent = { 0, 0 };
        bool                       invalid = false;

        std::vector<VkSemaphore> semaphores = {};
        u32                  semaphoreIndex = 0;

    public:
        ~SwapchainImpl();
    };

// -----------------------------------------------------------------------------

    struct QueueImpl : ImplBase
    {
        Context context = {};
        VkQueue  handle = {};
        u32      family = UINT32_MAX;
    };

// -----------------------------------------------------------------------------

    struct FenceImpl : ImplBase
    {
        Context context = {};

        VkSemaphore semaphore = {};
        u64             value = 0;

    public:
        ~FenceImpl();
    };

// -----------------------------------------------------------------------------

    struct DescriptorSetLayoutImpl : ImplBase
    {
        Context context = {};

        VkDescriptorSetLayout layout = {};
        u64                     size = 0;
        std::vector<u64>     offsets = {};

    public:
        ~DescriptorSetLayoutImpl();
    };

    struct DescriptorSetImpl : ImplBase
    {
        DescriptorSetLayout layout;

        VkDescriptorSet set;

    public:
        ~DescriptorSetImpl();
    };

// -----------------------------------------------------------------------------

    struct PipelineLayoutImpl : ImplBase
    {
        Context context = {};

        VkPipelineLayout layout = {};

        // TODO: Pipeline layout used in multiple bind points?
        BindPoint bindPoint = {};

        std::vector<VkPushConstantRange> ranges;
        std::vector<VkDescriptorSetLayout> sets;

    public:
        ~PipelineLayoutImpl();
    };

// -----------------------------------------------------------------------------

    struct CommandStateImpl : ImplBase
    {
        struct ImageState
        {
            VkImageLayout        layout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2       access = 0;

            u64 version = 0;
        };

    public:
        Context context = {};

        u64                                                   version = 0;
        ankerl::unordered_dense::map<VkImage, ImageState> imageStates;
        std::vector<VkImage>                                clearList;

        std::vector<Format> colorAttachmentsFormats;
        Format                depthAttachmentFormat = nova::Format::Undefined;
        Format              stencilAttachmentFormat = nova::Format::Undefined;
        Vec2U    renderingExtent;

    public:
        ImageState& Get(Texture texture) noexcept;
    };

// -----------------------------------------------------------------------------

    struct AccelerationStructureBuilderImpl : ImplBase
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

    public:
        ~AccelerationStructureBuilderImpl();

        void EnsureGeometries(u32 geometryIndex);
        void EnsureSizes();
    };

// -----------------------------------------------------------------------------

    struct AccelerationStructureImpl : ImplBase
    {
        Context context = {};

        VkAccelerationStructureKHR structure = {};
        u64                          address = {};
        VkAccelerationStructureTypeKHR  type = {};

        Buffer::Arc buffer = {};

    public:
        ~AccelerationStructureImpl();
    };

// -----------------------------------------------------------------------------

    struct RayTracingPipelineImpl : ImplBase
    {
        Context context = {};

        VkPipeline pipeline = {};
        Buffer::Arc    sbtBuffer = {};

        VkStridedDeviceAddressRegionKHR  rayGenRegion = {};
        VkStridedDeviceAddressRegionKHR rayMissRegion = {};
        VkStridedDeviceAddressRegionKHR  rayHitRegion = {};
        VkStridedDeviceAddressRegionKHR rayCallRegion = {};

    public:
        ~RayTracingPipelineImpl();
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

    struct GraphicsPipelineStateKey
    {
        std::array<VkFormat, 8> colorAttachments;
        VkFormat                 depthAttachment;
        VkFormat               stencilAttachment;

        Topology    topology;
        PolygonMode polyMode;
        u32  blendEnable : 1;

        std::array<VkShaderModule, 5> shaders;
        VkPipelineLayout               layout;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelineStateKey)
    };

    struct GraphicsPipelineVertexInputStageKey
    {
        VkPrimitiveTopology topology;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelineVertexInputStageKey)
    };

    struct GraphicsPipelinePreRasterizationStageKey
    {
        std::array<VkShaderModule, 4> shaders;
        VkPipelineLayout               layout;
        VkPolygonMode                polyMode;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelinePreRasterizationStageKey)
    };

    struct GraphicsPipelineFragmentShaderStageKey
    {
        VkShaderModule   shader;
        VkPipelineLayout layout;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelineFragmentShaderStageKey)
    };

    struct GraphicsPipelineFragmentOutputStageKey
    {
        std::array<VkFormat, 8> colorAttachments;
        VkFormat                 depthAttachment;
        VkFormat               stencilAttachment;

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
        VkShaderModule shader;
        VkPipelineLayout layout;

        NOVA_DEFINE_WYHASH_EQUALITY(ComputePipelineKey)
    };
}

NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelineStateKey);
NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelineVertexInputStageKey);
NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelinePreRasterizationStageKey);
NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelineFragmentShaderStageKey);
NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelineFragmentOutputStageKey);
NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelineLibrarySetKey);
NOVA_DEFINE_WYHASH_FOR(nova::ComputePipelineKey);

// -----------------------------------------------------------------------------

namespace nova
{
    struct CommandPoolImpl : ImplBase
    {
        Context context = {};
        Queue     queue = {};

        VkCommandPool                    pool = {};
        std::vector<CommandList::Arc> buffers = {};
        u32                             index = 0;

        std::vector<CommandList::Arc> secondaryBuffers = {};
        u32                             secondaryIndex = 0;

    public:
        ~CommandPoolImpl();
    };

    struct CommandListImpl : ImplBase
    {
        CommandPool       pool = {};
        CommandState     state = {};
        VkCommandBuffer buffer = {};
    };

// -----------------------------------------------------------------------------

    struct ContextImpl : ImplBase
    {
        static std::atomic_int64_t    AllocationCount;
        static std::atomic_int64_t NewAllocationCount;

    public:
        ContextConfig config = {};

        VkInstance  instance = {};
        VkPhysicalDevice gpu = {};
        VkDevice      device = {};
        VmaAllocator     vma = {};

        VkDebugUtilsMessengerEXT debugMessenger = {};

    public: // Device properties

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

        Queue::Arc graphics = {};

    public: // Pipeline cache

        std::shared_mutex                                              pipelineMutex;
        ankerl::unordered_dense::map<GraphicsPipelineStateKey, VkPipeline> pipelines;

        bool usePipelineLibraries = true;

        ankerl::unordered_dense::map<GraphicsPipelineVertexInputStageKey, VkPipeline>       vertexInputStages;
        ankerl::unordered_dense::map<GraphicsPipelinePreRasterizationStageKey, VkPipeline>    preRasterStages;
        ankerl::unordered_dense::map<GraphicsPipelineFragmentShaderStageKey, VkPipeline> fragmentShaderStages;
        ankerl::unordered_dense::map<GraphicsPipelineFragmentOutputStageKey, VkPipeline> fragmentOutputStages;
        ankerl::unordered_dense::map<GraphicsPipelineLibrarySetKey, VkPipeline>          graphicsPipelineSets;

        ankerl::unordered_dense::map<ComputePipelineKey, VkPipeline> computePipelines;

        ankerl::unordered_dense::set<VkShaderModule>          deletedShaders;
        ankerl::unordered_dense::set<VkPipeline>            deletedPipelines;
        VkPipelineCache                                        pipelineCache = {};

    public: // Descriptor Pool

        std::shared_mutex descriptorPoolMutex;
        VkDescriptorPool       descriptorPool = {};

    public: // Allocation callbacks

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

    public:
        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT* data,
            void* userData);
    };
}
