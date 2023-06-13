#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_ImplHandle.hpp>

#include <nova/core/nova_Math.hpp>

// #define NOVA_NOISY_VULKAN_ALLOCATIONS

namespace nova
{
    inline std::atomic<u64> TimeSubmitting = 0;
    inline std::atomic<u64> TimeAdaptingFromAcquire = 0;
    inline std::atomic<u64> TimeAdaptingToPresent = 0;
    inline std::atomic<u64> TimePresenting = 0;
    inline std::atomic<u64> TimeSettingGraphicsState = 0;

    inline
    void VkCall(VkResult res)
    {
        if (res != VK_SUCCESS)
            NOVA_THROW("Error: {}", int(res));
    }

    template<class Container, class Fn, class ... Args>
    void VkQuery(Container&& container, Fn&& fn, Args&& ... args)
    {
        u32 count;
        fn(std::forward<Args>(args)..., &count, nullptr);
        container.resize(count);
        fn(std::forward<Args>(args)..., &count, container.data());
    }

// -----------------------------------------------------------------------------

    NOVA_DECLARE_HANDLE_OBJECT(Buffer)
    NOVA_DECLARE_HANDLE_OBJECT(CommandList)
    NOVA_DECLARE_HANDLE_OBJECT(CommandPool)
    NOVA_DECLARE_HANDLE_OBJECT(Context)
    NOVA_DECLARE_HANDLE_OBJECT(DescriptorSet)
    NOVA_DECLARE_HANDLE_OBJECT(DescriptorSetLayout)
    NOVA_DECLARE_HANDLE_OBJECT(Fence)
    NOVA_DECLARE_HANDLE_OBJECT(PipelineLayout)
    NOVA_DECLARE_HANDLE_OBJECT(Queue)
    NOVA_DECLARE_HANDLE_OBJECT(CommandState)
    NOVA_DECLARE_HANDLE_OBJECT(Sampler)
    NOVA_DECLARE_HANDLE_OBJECT(Shader)
    NOVA_DECLARE_HANDLE_OBJECT(Surface)
    NOVA_DECLARE_HANDLE_OBJECT(Swapchain)
    NOVA_DECLARE_HANDLE_OBJECT(Texture)
    NOVA_DECLARE_HANDLE_OBJECT(AccelerationStructure)
    NOVA_DECLARE_HANDLE_OBJECT(AccelerationStructureBuilder)
    NOVA_DECLARE_HANDLE_OBJECT(RayTracingPipeline)

    NOVA_DECLARE_HANDLE_OBJECT(GraphicsPipeline)
    NOVA_DECLARE_HANDLE_OBJECT(PipelineCache)

// -----------------------------------------------------------------------------

    enum class BufferFlags : u32
    {
        None,
        Addressable  = 1 << 0,
        DeviceLocal  = 1 << 1 | Addressable,
        Mappable     = 1 << 2,
        CreateMapped = 1 << 3 | Mappable,
    };
    NOVA_DECORATE_FLAG_ENUM(BufferFlags)

    enum class BufferUsage : u32
    {
        TransferSrc = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        TransferDst = VK_BUFFER_USAGE_TRANSFER_DST_BIT,

        Uniform = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        Storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,

        Index = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        Vertex = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,

        Indirect = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,

        ShaderBindingTable = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,

        AccelBuild = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        AccelStorage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,

        DescriptorSamplers = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
        DescriptorResources = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
    };
    NOVA_DECORATE_FLAG_ENUM(BufferUsage)

    enum class TextureFlags : u32
    {
        Array = 1 << 0,
        Mips  = 1 << 1,
    };
    NOVA_DECORATE_FLAG_ENUM(TextureFlags)

    enum class TextureUsage : u32
    {
        TransferSrc        = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        TransferDst        = VK_IMAGE_USAGE_TRANSFER_DST_BIT,

        Sampled            = VK_IMAGE_USAGE_SAMPLED_BIT,
        Storage            = VK_IMAGE_USAGE_STORAGE_BIT,

        ColorAttach        = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        DepthStencilAttach = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    };
    NOVA_DECORATE_FLAG_ENUM(TextureUsage)

    enum class Format : u32
    {
        Undefined = VK_FORMAT_UNDEFINED,

        RGBA8U = VK_FORMAT_R8G8B8A8_UNORM,
        RGBA16F = VK_FORMAT_R16G16B16A16_SFLOAT,
        RGBA32F = VK_FORMAT_R32G32B32A32_SFLOAT,

        RGB32F = VK_FORMAT_R32G32B32_SFLOAT,

        R8U = VK_FORMAT_R8_UNORM,
        R32F = VK_FORMAT_R32_SFLOAT,

        R8UInt = VK_FORMAT_R8_UINT,
        R16UInt = VK_FORMAT_R16_UINT,
        R32UInt = VK_FORMAT_R32_UINT,

        D24U_X8 = VK_FORMAT_X8_D24_UNORM_PACK32,
        D24U_S8 = VK_FORMAT_D24_UNORM_S8_UINT,

        D32 = VK_FORMAT_D32_SFLOAT,
    };

    enum class IndexType : u32
    {
        U16 = VK_INDEX_TYPE_UINT16,
        U32 = VK_INDEX_TYPE_UINT32,
        U8 = VK_INDEX_TYPE_UINT8_EXT,
    };

    enum class Filter : u32
    {
        Linear = VK_FILTER_LINEAR,
        Nearest = VK_FILTER_NEAREST,
    };

    enum class AddressMode : u32
    {
        Repeat = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        RepeatMirrored = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        Edge = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        Border = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    };

    enum class BorderColor : u32
    {
        TransparentBlack = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        Black = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        White = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    };

    enum class ShaderStage : u32
    {
        None = 0,

        Vertex = VK_SHADER_STAGE_VERTEX_BIT,
        TessControl = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        TessEval = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        Geometry = VK_SHADER_STAGE_GEOMETRY_BIT,
        Fragment = VK_SHADER_STAGE_FRAGMENT_BIT,

        Compute = VK_SHADER_STAGE_COMPUTE_BIT,

        RayGen = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        AnyHit = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        ClosestHit = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        Miss = VK_SHADER_STAGE_MISS_BIT_KHR,
        Intersection = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
    };
    NOVA_DECORATE_FLAG_ENUM(ShaderStage)

    enum class PresentMode : u32
    {
        Immediate   = VK_PRESENT_MODE_IMMEDIATE_KHR,
        Mailbox     = VK_PRESENT_MODE_MAILBOX_KHR,
        Fifo        = VK_PRESENT_MODE_FIFO_KHR,
        FifoRelaxed = VK_PRESENT_MODE_FIFO_RELAXED_KHR,
    };

    enum class ResourceState
    {
        Sampled,
        GeneralImage,
        ColorAttachment,
        DepthStencilAttachment,
        Present,
    };

    enum class PipelineStage
    {
        Compute = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        Graphics = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        RayTracing = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
    };

    enum class BindPoint : u32
    {
        // TODO: Support transfer
        Graphics = VK_PIPELINE_BIND_POINT_GRAPHICS,
        Compute = VK_PIPELINE_BIND_POINT_COMPUTE,
        RayTracing = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
    };

    enum class AccelerationStructureType : u32
    {
        BottomLevel = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        TopLevel = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    };

    enum class AccelerationStructureFlags : u32
    {
        PreferFastTrace = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        PreferFastBuild = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,
        AllowDataAccess = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR,
        AllowCompaction = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
    };
    NOVA_DECORATE_FLAG_ENUM(AccelerationStructureFlags)

    enum class GeometryInstanceFlags : u32
    {
        TriangleCullClockwise        = 1 << 0,
        TriangleCullCounterClockwise = 1 << 1,
        InstanceForceOpaque          = 1 << 2,
    };
    NOVA_DECORATE_FLAG_ENUM(GeometryInstanceFlags)

    enum class DescriptorType : u32
    {
        SampledTexture = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        StorageTexture = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        Uniform = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        Storage = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        AccelerationStructure = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    };

    enum class CompareOp : u32
    {
        Never          = VK_COMPARE_OP_NEVER,
        Less           = VK_COMPARE_OP_LESS,
        Equal          = VK_COMPARE_OP_EQUAL,
        LessOrEqual    = VK_COMPARE_OP_LESS_OR_EQUAL,
        Greater        = VK_COMPARE_OP_GREATER,
        NotEqual       = VK_COMPARE_OP_NOT_EQUAL,
        GreaterOrEqual = VK_COMPARE_OP_GREATER_OR_EQUAL,
        Always         = VK_COMPARE_OP_ALWAYS,
    };

    enum class CullMode : u32
    {
        None  = VK_CULL_MODE_NONE,
        Front = VK_CULL_MODE_FRONT_BIT,
        Back  = VK_CULL_MODE_BACK_BIT,
    };
    NOVA_DECORATE_FLAG_ENUM(CullMode);

    enum class FrontFace : u32
    {
        CounterClockwise = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        Clockwise = VK_FRONT_FACE_CLOCKWISE,
    };

    enum class PolygonMode : u32
    {
        Fill = VK_POLYGON_MODE_FILL,
        Line = VK_POLYGON_MODE_LINE,
        Point = VK_POLYGON_MODE_POINT
    };

    enum class Topology : u32
    {
        Points = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        Lines = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
        LineStrip = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
        Triangles = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        TriangleStrip = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        TriangleFan = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
        Patches = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
    };

    enum class CommandListType
    {
        Primary,
        Secondary,
    };

    enum class QueueFlags : u32
    {
        Graphics = VK_QUEUE_GRAPHICS_BIT,
        Compute  = VK_QUEUE_COMPUTE_BIT,
        Transfer = VK_QUEUE_TRANSFER_BIT
    };
    NOVA_DECORATE_FLAG_ENUM(QueueFlags)

// -----------------------------------------------------------------------------

    struct Buffer : ImplHandle<BufferImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(Buffer)

    public:
        Buffer(Context context, u64 size, BufferUsage usage, BufferFlags flags = {});

    private:
        void* Get_(u64 index, u64 offset, usz stride) const noexcept;
        void Set_(const void* data, usz count, u64 index, u64 offset, usz stride) const noexcept;
    public:
        void Resize(u64 size) const;

        u64 GetSize() const noexcept;
        b8* GetMapped() const noexcept;
        u64 GetAddress() const noexcept;

        template<class T>
        T& Get(u64 index, u64 offset = 0) const noexcept
        {
            constexpr auto Stride = AlignUpPower2(sizeof(T), alignof(T));
            return *reinterpret_cast<T*>(Get_(index, offset, Stride));
        }

        template<class T>
        void Set(Span<T> elements, u64 index = 0, u64 offset = 0) const noexcept
        {
            constexpr auto Stride = AlignUpPower2(sizeof(T), alignof(T));
            Set_(elements.data(), elements.size(), index, offset, Stride);
        }
    };

// -----------------------------------------------------------------------------

    struct Sampler : ImplHandle<SamplerImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(Sampler)

    public:
        Sampler(Context context, Filter filter, AddressMode addressMode, BorderColor color, f32 anistropy = 0.f);
    };

// -----------------------------------------------------------------------------

    struct Texture : ImplHandle<TextureImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(Texture)

    public:
        Texture(Context context, Vec3U size, TextureUsage usage, Format format, TextureFlags flags = {});

    public:
        Vec3U GetExtent() const noexcept;
        Format GetFormat() const noexcept;
    };

// -----------------------------------------------------------------------------

    struct Shader : ImplHandle<ShaderImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(Shader)

    public:
        Shader(Context context, ShaderStage stage, const std::string& filename, const std::string& sourceCode = {});

        struct ShaderStageInfo
        {
            ShaderStage stage;
            std::string_view filename;
            std::string_view sourceCode;
        };
        Shader(Context context, PipelineLayout layout, Span<ShaderStageInfo> stage);

    public:
        VkPipelineShaderStageCreateInfo GetStageInfo() const noexcept;
    };

// -----------------------------------------------------------------------------

    struct Surface : ImplHandle<SurfaceImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(Surface)

    public:
        Surface(Context context, void* handle);
    };

// -----------------------------------------------------------------------------

    struct Swapchain : ImplHandle<SwapchainImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(Swapchain)

    public:
        Swapchain(Context context, Surface surface, TextureUsage usage, PresentMode presentMode);

    public:
        Texture GetCurrent() const noexcept;
        Vec2U GetExtent() const noexcept;
        Format GetFormat() const noexcept;
    };

// -----------------------------------------------------------------------------

    struct Queue : ImplHandle<QueueImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(Queue)

    public:
        Queue(Context context, VkQueue queue, u32 family);

    public:
        void Submit(Span<CommandList> commandLists, Span<Fence> waits, Span<Fence> signals) const;
        bool Acquire(Span<Swapchain> swapchains, Span<Fence> signals) const;

        // Present a set of swapchains, waiting on a number of fences.
        // If any wait dependency includes a wait-before-signal operation
        // (including indirectly) then hostWait must be set to true, as WSI
        // operations are incompatible with wait-before-signal.
        void Present(Span<Swapchain> swapchains, Span<Fence> waits, bool hostWait = false) const;
    };

// -----------------------------------------------------------------------------

    struct Fence : ImplHandle<FenceImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(Fence)

    public:
        Fence(Context context);

    public:
        void Wait(u64 waitValue = 0ull) const;
        u64 Advance() const noexcept;
        void Signal(u64 signalValue = 0ull) const;
    };

// -----------------------------------------------------------------------------

    struct DescriptorBinding
    {
        DescriptorType type;
        u32           count = 1;
    };

    struct DescriptorSetBindingOffset
    {
        u32 buffer;
        u64 offset = {};
    };

    struct DescriptorSetLayout : ImplHandle<DescriptorSetLayoutImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(DescriptorSetLayout)

    public:
        DescriptorSetLayout(Context context, Span<DescriptorBinding> bindings, bool pushDescriptor = false);

    public:
        u64 GetSize() const noexcept;
        void WriteSampledTexture(void* dst, u32 binding, Texture texture, Sampler sampler, u32 arrayIndex = 0) const noexcept;
    };

    struct DescriptorSet : ImplHandle<DescriptorSetImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(DescriptorSet)

    public:
        DescriptorSet(DescriptorSetLayout layout, u64 customSize = 0);

    public:
        void WriteSampledTexture(u32 binding, Texture texture, Sampler sampler, u32 arrayIndex = 0) const noexcept;
    };

// -----------------------------------------------------------------------------

    struct PushConstantRange
    {
        ShaderStage stages;
        u32           size;
        u32         offset = 0;
    };

    struct PipelineLayout : ImplHandle<PipelineLayoutImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(PipelineLayout)

    public:
        PipelineLayout(Context context,
            Span<PushConstantRange> pushConstantRanges,
            Span<DescriptorSetLayout> descriptorLayouts,
            BindPoint bindPoint);
    };

// -----------------------------------------------------------------------------

    struct CommandState : ImplHandle<CommandStateImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(CommandState)

    public:
        CommandState(Context context);

    public:
        void Clear(u32 maxAge) const noexcept;

        void Reset(Texture texture) const noexcept;
        void Persist(Texture texture) const noexcept;
        void Set(Texture texture, VkImageLayout layout, VkPipelineStageFlags2 stage, VkAccessFlags2 access) const noexcept;
    };

// -----------------------------------------------------------------------------

    struct AccelerationStructureBuilder : ImplHandle<AccelerationStructureBuilderImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(AccelerationStructureBuilder)

    public:
        AccelerationStructureBuilder(Context context);

    public:
        void SetInstances(u32 geometryIndex, u64 deviceAddress, u32 count) const;
        void SetTriangles(u32 geometryIndex,
            u64 vertexAddress, Format vertexFormat, u32 vertexStride, u32 maxVertex,
            u64 indexAddress, IndexType indexFormat, u32 triangleCount) const;

        void Prepare(nova::AccelerationStructureType type, nova::AccelerationStructureFlags flags,
            u32 geometryCount, u32 firstGeometry = 0u) const;

        u64 GetInstanceSize() const;
        void WriteInstance(
            void* bufferAddress, u32 index,
            AccelerationStructure& structure,
            const Mat4& matrix,
            u32 customIndex, u8 mask,
            u32 sbtOffset, GeometryInstanceFlags flags) const;

        u64 GetBuildSize() const;
        u64 GetBuildScratchSize() const;
        u64 GetUpdateScratchSize() const;
        u64 GetCompactSize() const;
    };

// -----------------------------------------------------------------------------

    struct AccelerationStructure : ImplHandle<AccelerationStructureImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(AccelerationStructure)

    public:
        AccelerationStructure(Context context, usz size, AccelerationStructureType type);

    public:
        u64 GetAddress() const noexcept;
    };

// -----------------------------------------------------------------------------

    struct HitShaderGroup
    {
        Shader closestHitShader = {};
        Shader anyHitShader = {};
        Shader intersectionShader = {};
    };

    struct RayTracingPipeline : ImplHandle<RayTracingPipelineImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(RayTracingPipeline)

    public:
        RayTracingPipeline(Context context);

    public:
        void Update(
            PipelineLayout layout,
            Span<Shader> rayGenShaders,
            Span<Shader> rayMissShaders,
            Span<HitShaderGroup> rayHitShaderGroup,
            Span<Shader> callableShaders) const;
    };

// -----------------------------------------------------------------------------

    struct RenderingDescription
    {
        Span<Format> colorFormats;
        Format        depthFormat = {};
        Format      stencilFormat = {};
        u32               subpass = 0;
    };

    struct PipelineState
    {
        Topology      topology = Topology::Triangles;         // VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT (partial - dynamic within class)
        CullMode      cullMode = CullMode::Back;              // VK_DYNAMIC_STATE_CULL_MODE_EXT
        FrontFace    frontFace = FrontFace::CounterClockwise; // VK_DYNAMIC_STATE_FRONT_FACE_EXT
        PolygonMode   polyMode = PolygonMode::Fill;           // N/A
        f32          lineWidth = 1.f;                         // VK_DYNAMIC_STATE_LINE_WIDTH
        CompareOp depthCompare = CompareOp::Greater;          // VK_DYNAMIC_STATE_DEPTH_COMPARE_OP
        u32    blendEnable : 1 = false;                       // N/A
        u32    depthEnable : 1 = false;                       // VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE
        u32     depthWrite : 1 = true;                        // VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE
        u32   flipVertical : 1 = false;
    };

// -----------------------------------------------------------------------------

    struct CommandPool : ImplHandle<CommandPoolImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(CommandPool)

    public:
        CommandPool(Context context, Queue queue);

    public:
        CommandList Begin(CommandState state) const;
        CommandList BeginSecondary(CommandState state, OptRef<const RenderingDescription> renderingDescription = {}) const;

        void Reset() const;
    };

    struct CommandList : ImplHandle<CommandListImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(CommandList)

    public:
        void End() const;

        void UpdateBuffer(Buffer dst, const void* pData, usz size, u64 dstOffset = 0) const;
        void CopyToBuffer(Buffer dst, Buffer src, u64 size, u64 dstOffset = 0, u64 srcOffset = 0) const;
        void CopyToTexture(Texture dst, Buffer src, u64 srcOffset = 0) const;
        void GenerateMips(Texture texture) const;
        void BlitImage(Texture dst, Texture src, Filter filter) const;

        void Clear(Texture texture, Vec4 color) const;
        void Transition(Texture texture, VkImageLayout newLayout, VkPipelineStageFlags2 newStages, VkAccessFlags2 newAccess) const;
        void Transition(Texture texture, ResourceState state, PipelineStage stages) const;
        void Present(Texture texture) const;

        void SetViewport(Vec2U size, bool flipVertical) const;

        void SetGraphicsState(PipelineLayout layout, Span<Shader> shaders, const PipelineState& state) const;
        void SetComputeState(PipelineLayout layout, Shader shader) const;

        void BeginRendering(Span<Texture> colorAttachments, Texture depthAttachment = {}, Texture stencilAttachment = {}, bool allowSecondary = false) const;
        void EndRendering() const;
        void ClearColor(u32 attachment, Vec4 color, Vec2U size, Vec2I offset = {}) const;
        void ClearDepth(f32 depth, Vec2U size, Vec2I offset = {}) const;
        void ClearStencil(u32 value, Vec2U size, Vec2I offset = {}) const;

        void BindDescriptorSets(PipelineLayout pipelineLayout, u32 firstSet, Span<DescriptorSet> sets) const;
        void BindDescriptorBuffers(Span<Buffer> buffers) const;
        void SetDescriptorSetOffsets(PipelineLayout layout, u32 firstSet, Span<DescriptorSetBindingOffset> offsets) const;

        void BindIndexBuffer(Buffer buffer, IndexType indexType, u64 offset = 0) const;
        void PushConstants(PipelineLayout layout, ShaderStage stages, u64 offset, u64 size, const void* data) const;

        void PushStorageTexture(PipelineLayout layout, u32 setIndex, u32 binding, Texture texture, u32 arrayIndex = 0) const;
        void PushAccelerationStructure(PipelineLayout layout, u32 setIndex, u32 binding, AccelerationStructure accelerationStructure, u32 arrayIndex = 0) const;

        void Dispatch(Vec3U groups) const;

        void Draw(u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance) const;
        void DrawIndexed(u32 indices, u32 instances, u32 firstIndex, u32 vertexOffset, u32 firstInstance) const;
        void ExecuteCommands(Span<CommandList> commands) const;

        void BuildAccelerationStructure(AccelerationStructureBuilder builder, AccelerationStructure structure, Buffer scratch) const;
        void CompactAccelerationStructure(AccelerationStructure dst, AccelerationStructure src) const;
        void BindPipeline(RayTracingPipeline pipeline) const;
        void TraceRays(RayTracingPipeline pipeline, Vec3U extent, u32 genIndex) const;
    };

// -----------------------------------------------------------------------------

    struct ContextConfig
    {
        bool debug = false;
        bool rayTracing = false;
        bool shaderObjects = false;
        bool descriptorBuffers = false;
    };

    struct Context : ImplHandle<ContextImpl>
    {
        NOVA_DECLARE_HANDLE_OPERATIONS(Context)

    public:
        Context(const ContextConfig& config);

    public:
        void WaitForIdle() const;

        Queue GetQueue(QueueFlags flags) const noexcept;

        void CleanPipelines() const;
    };
}
