#pragma once

#include <nova/rhi/nova_RHI.hpp>

#include <nova/core/nova_Registry.hpp>
#include <nova/core/nova_Array.hpp>

namespace nova
{
    enum class UID : u64 {
        Invalid = 0,
    };

    struct Context;

    struct Queue : Object
    {
        VkQueue               handle = {};
        u32                   family = UINT32_MAX;
        VkPipelineStageFlags2 stages = {};
    
    public:
        Queue(HContext context);
        ~Queue() final;
    
        void Submit(Span<HCommandList>, Span<HFence> waits, Span<HFence> signals);
        bool Acquire(Span<HSwapchain>, Span<HFence> signals);
        void Present(Span<HSwapchain>, Span<HFence> waits, bool hostWait = false);
    };

    struct Fence : Object
    {
        VkSemaphore semaphore = {};
        u64             value = 0;
    
    public:
        Fence(HContext);
        ~Fence() final;

        void Wait(u64 waitValue = ~0ull);
        u64 Advance();
        void Signal(u64 signalValue = ~0ull);
        u64 GetPendingValue();
    };

    struct CommandPool : Object
    {
        HQueue queue = {};

        VkCommandPool                              pool = {};
        std::vector<std::unique_ptr<CommandList>> lists = {};
        u32                                       index = 0;
    
    public:
        CommandPool(HContext, HQueue);
        ~CommandPool() final;

        HCommandList Begin(HCommandState);
        void Reset();
    };

    struct CommandState : Object
    {
        struct ImageState
        {
            VkImageLayout        layout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2       access = 0;

            u64 version = 0;
        };

        ankerl::unordered_dense::map<VkImage, ImageState> imageStates;

        std::vector<Format> colorAttachmentsFormats;
        Format                depthAttachmentFormat = nova::Format::Undefined;
        Format              stencilAttachmentFormat = nova::Format::Undefined;
        Vec2U                       renderingExtent;
    
    public:
        CommandState(HContext);
        ~CommandState() final;

        void SetState(HTexture texture, VkImageLayout layout, VkPipelineStageFlags2 stages, VkAccessFlags2 access);
    };

    struct CommandList
    {
        HCommandPool      pool = {};
        HCommandState    state = {};
        VkCommandBuffer buffer = {};
    
    public:
        CommandList() = default;
        CommandList(const CommandList&) = delete;
        CommandList(CommandList&&) = delete;

        void Present(HSwapchain swapchain);

        void SetGraphicsState(HPipelineLayout layout, Span<HShader> shaders, const PipelineState& state);
        void PushConstants(HPipelineLayout layout, u64 offset, u64 size, const void* data);

        void Barrier(PipelineStage src, PipelineStage dst);

        void BeginRendering(Rect2D region, Span<HTexture> colorAttachments, HTexture = {}, HTexture stencilAttachment = {});
        void EndRendering();
        void Draw(u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance);
        void DrawIndexed(u32 indices, u32 instances, u32 firstIndex, u32 vertexOffset, u32 firstInstance);
        void BindIndexBuffer(HBuffer buffer, IndexType indexType, u64 offset = 0);
        void ClearColor(u32 attachment, Vec4 color, Vec2U size, Vec2I offset = {});
        void ClearDepth(f32 depth, Vec2U size, Vec2I offset = {});
        void ClearStencil(u32 value, Vec2U size, Vec2I offset = {});

        void SetComputeState(HPipelineLayout layout, HShader shader);
        void Dispatch(Vec3U groups);

        void BindDescriptorSets(HPipelineLayout layout, u32 firstSet, Span<HDescriptorSet> sets);

        void PushStorageTexture(HPipelineLayout layout, u32 setIndex, u32 binding, HTexture texture, u32 arrayIndex = 0);
        void PushAccelerationStructure(HPipelineLayout layout, u32 setIndex, u32 binding, HAccelerationStructure accelStructure, u32 arrayIndex = 0);

        void UpdateBuffer(HBuffer dst, const void* data, usz size, u64 dstOffset = 0);
        void CopyToBuffer(HBuffer dst, HBuffer src, u64 size, u64 dstOffset = 0, u64 srcOffset = 0);
        void Barrier(HBuffer buffer, PipelineStage src, PipelineStage dst);
        
        void Transition(HTexture texture, VkImageLayout newLayout, VkPipelineStageFlags2 newStages, VkAccessFlags2 newAccess);
        void Transition(HTexture texture, TextureLayout newLayout, PipelineStage newStages);
        void Clear(HTexture texture, Vec4 color);
        void CopyToTexture(HTexture dst, HBuffer src, u64 srcOffset = 0);
        void CopyFromTexture(HBuffer dst, HTexture src, Rect2D region);
        void GenerateMips(HTexture mips);
        void BlitImage(HTexture dst, HTexture src, Filter filter);

        void BuildAccelerationStructure(HAccelerationStructureBuilder builder, HAccelerationStructure structure, HBuffer scratch);
        void CompactAccelerationStructure(HAccelerationStructure dst, HAccelerationStructure src);

        void TraceRays(HRayTracingPipeline pipeline, Vec3U extent, u32 genIndex);
    };

    struct Swapchain : Object
    {
        VkSurfaceKHR                           surface = {};
        VkSwapchainKHR                       swapchain = {};
        VkSurfaceFormatKHR                      format = { VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        TextureUsage                             usage = {};
        PresentMode                        presentMode = PresentMode::Fifo;
        std::vector<std::unique_ptr<Texture>> textures = {};
        uint32_t                                 index = UINT32_MAX;
        VkExtent2D                              extent = { 0, 0 };
        bool                                   invalid = false;

        std::vector<VkSemaphore> semaphores = {};
        u32                  semaphoreIndex = 0;

    public:
        Swapchain(HContext, void* window, TextureUsage usage, PresentMode mode);
        ~Swapchain() final;

        HTexture GetCurrent();
        Vec2U GetExtent();
        Format GetFormat();
    };

    struct PipelineLayout : Object
    {
        UID id = UID::Invalid;

        VkPipelineLayout layout;

        // TODO: Pipeline layout used in multiple bind points?
        BindPoint bindPoint = {};

        std::vector<PushConstantRange>     pcRanges;
        std::vector<HDescriptorSetLayout> setLayouts;

        std::vector<VkPushConstantRange> ranges;
        std::vector<VkDescriptorSetLayout> sets;
    
    public:
        PipelineLayout(HContext, Span<PushConstantRange>, Span<HDescriptorSetLayout>, BindPoint);
        ~PipelineLayout() final;
    };

    struct DescriptorSetLayout : Object
    {
        std::vector<DescriptorBinding> bindings = {};

        VkDescriptorSetLayout layout = {};
        u64                     size = 0;
        std::vector<u64>     offsets = {};
    
    public:
        DescriptorSetLayout(HContext, Span<DescriptorBinding> bindings, bool pushDescriptors = false);
        ~DescriptorSetLayout();
    };

    struct DescriptorSet : Object
    {
        HDescriptorSetLayout layout;
        VkDescriptorSet         set;
    
    public:
        DescriptorSet(HDescriptorSetLayout, u64 customSize = 0);
        ~DescriptorSet() final;

        void WriteSampledTexture(u32 binding, HTexture texture, HSampler sampler, u32 arrayIndex = 0);
        void WriteUniformBuffer(u32 binding, HBuffer buffer, u32 arrayIndex = 0);
    };

    struct Shader : Object
    {
        UID id = UID::Invalid;

        VkShaderModule handle = {};
        ShaderStage     stage = {};
    
    public:
        Shader(HContext, ShaderStage stage, const std::string& filename, const std::string& sourceCode);
        Shader(HContext, ShaderStage stage, Span<ShaderElement> elements);
        ~Shader() final;

        VkPipelineShaderStageCreateInfo GetStageInfo();
    };

    struct Buffer : Object
    {
        VkBuffer          buffer = {};
        VmaAllocation allocation = {};
        VkDeviceSize        size = 0ull;
        VkDeviceAddress  address = 0ull;
        BufferFlags        flags = BufferFlags::None;
        BufferUsage        usage = {};
    
    public:
        Buffer(HContext, u64 size, BufferUsage usage, BufferFlags flags = {});
        ~Buffer() final;

        void Resize(u64 size);
        u64 GetSize();
        b8* GetMapped();
        u64 GetAddress();

        template<typename T>
        T& Get(u64 index, u64 offset = 0)
        {
            return reinterpret_cast<T*>(GetMapped() + offset)[index];
        }

        template<typename T>
        void Set(Span<T> elements, u64 index = 0, u64 offset = 0)
        {
            T* dst = reinterpret_cast<T*>(GetMapped() + offset) + index;
            std::memcpy(dst, elements.data(), elements.size() * sizeof(T));
        }
    };

    struct Sampler : Object
    {
        VkSampler sampler;
    
    public:
        Sampler(HContext, Filter filter, AddressMode addressMode, BorderColor color, f32 anisotropy = 0.f);
        ~Sampler() final;
    };

    struct Texture : Object
    {
        VkImage             image = {};
        VmaAllocation  allocation = {};
        VkImageView          view = {};
        TextureUsage        usage = {};
        Format             format = Format::Undefined;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

        Vec3U extent = {};
        u32     mips = 0;
        u32   layers = 0;
    
    public:
        Texture(HContext, Vec3U size, TextureUsage usage, Format format, TextureFlags flags = {});
        Texture(HContext, VkImage, VmaAllocation, VkImageView, TextureUsage, Format, VkImageAspectFlags, Vec3U extent, u32 mips, u32 layers);
        ~Texture() final;

        Vec3U GetExtent();
        Format GetFormat();
    };

    struct AccelerationStructureBuilder : Object
    {
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
        AccelerationStructureBuilder(HContext);
        ~AccelerationStructureBuilder() final;

        void SetInstances(u32 geometryIndex, u64 deviceAddress, u32 count);
        void SetTriangles(u32 geometryIndex,
            u64 vertexAddress, Format vertexFormat, u32 vertexStride, u32 maxVertex,
            u64 indexAddress, IndexType indexFormat, u32 triangleCount);

        void Prepare(AccelerationStructureType type, AccelerationStructureFlags flags,
            u32 geometryCount, u32 firstGeometry = 0u);
        
        u32 GetInstanceSize();
        void WriteInstance(
            void* bufferAddress, u32 index,
            HAccelerationStructure structure,
            const Mat4& matrix,
            u32 customIndex, u8 mask,
            u32 sbtOffset, GeometryInstanceFlags flags);
        
        u64 GetBuildSize();
        u64 GetBuildScratchSize();
        u64 GetUpdateScratchSize();
        u64 GetCompactSize();
    };

    struct AccelerationStructure : Object
    {
        VkAccelerationStructureKHR structure = {};
        u64                          address = {};
        VkAccelerationStructureTypeKHR  type = {};

        HBuffer buffer;
        bool ownBuffer;
    
    public:
        AccelerationStructure(HContext, u64 size, AccelerationStructureType type, HBuffer buffer = {}, u64 offset = 0);
        ~AccelerationStructure() final;

        u64 GetAddress();
    };

    struct RayTracingPipeline : Object
    {
        VkPipeline               pipeline = {};
        std::unique_ptr<Buffer> sbtBuffer = {};

        VkStridedDeviceAddressRegionKHR  rayGenRegion = {};
        VkStridedDeviceAddressRegionKHR rayMissRegion = {};
        VkStridedDeviceAddressRegionKHR  rayHitRegion = {};
        VkStridedDeviceAddressRegionKHR rayCallRegion = {};
    
    public:
        RayTracingPipeline(HContext);
        ~RayTracingPipeline() final;

        void Update(HPipelineLayout layout,
            Span<HShader> rayGenShaders,
            Span<HShader> rayMissShaders,
            Span<HitShaderGroup> rayHitShaderGroup,
            Span<HShader> callableShaders);
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
        UID                 layout;
        PolygonMode       polyMode;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelinePreRasterizationStageKey)
    };

    struct GraphicsPipelineFragmentShaderStageKey
    {
        UID shader;
        UID layout;

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
        UID layout;

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

    struct Context
    {
        ContextConfig config = {};

        VkInstance  instance = {};
        VkPhysicalDevice gpu = {};
        VkDevice      device = {};
        VmaAllocator     vma = {};

        VkDebugUtilsMessengerEXT debugMessenger = {};

    public:
        VkDescriptorPool descriptorPool = {};

        std::vector<std::unique_ptr<Queue>>  graphicQueues = {};
        std::vector<std::unique_ptr<Queue>> transferQueues = {};
        std::vector<std::unique_ptr<Queue>>  computeQueues = {};

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
        static std::atomic_int64_t    AllocationCount;
        static std::atomic_int64_t NewAllocationCount;

        VkAllocationCallbacks alloc = {
            .pfnAllocation = +[](void*, size_t size, size_t align, [[maybe_unused]] VkSystemAllocationScope scope) {
                void* ptr = mi_malloc_aligned(size, align);
                if (ptr)
                {
                    MemoryAllocated += mi_usable_size(ptr);
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
                    MemoryAllocated -= mi_usable_size(ptr);
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
        Context(const ContextConfig& config);
        ~Context();

        void WaitIdle();
        const ContextConfig& GetConfig();

        HQueue GetQueue(QueueFlags flags, u32 index);
    };
}