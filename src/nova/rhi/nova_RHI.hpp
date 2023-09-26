#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Ref.hpp>
#include <nova/core/nova_Math.hpp>

// TODO: No! >:(
#include <volk.h>
#include <vk_mem_alloc.h>

// #define NOVA_NOISY_VULKAN_ALLOCATIONS

namespace nova
{
    namespace rhi::stats
    {
        inline std::atomic<u64> TimeSubmitting = 0;
        inline std::atomic<u64> TimeAdaptingFromAcquire = 0;
        inline std::atomic<u64> TimeAdaptingToPresent = 0;
        inline std::atomic<u64> TimePresenting = 0;
        inline std::atomic<u64> TimeSettingGraphicsState = 0;

        inline std::atomic<i64> AllocationCount = 0;
        inline std::atomic<i64> NewAllocationCount = 0;
        inline std::atomic<u64> MemoryAllocated = 0;
    }

// -----------------------------------------------------------------------------

    using HContext = Handle<struct Context>;
    using HBuffer = Handle<struct Buffer>;
    using HCommandList = Handle<struct CommandList>;
    using HCommandPool = Handle<struct CommandPool>;
    using HDescriptorHeap = Handle<struct DescriptorHeap>;
    using HFence = Handle<struct Fence>;
    using HQueue = Handle<struct Queue>;
    using HSampler = Handle<struct Sampler>;
    using HShader = Handle<struct Shader>;
    using HSwapchain = Handle<struct Swapchain>;
    using HTexture = Handle<struct Texture>;
    using HAccelerationStructure = Handle<struct AccelerationStructure>;
    using HAccelerationStructureBuilder = Handle<struct AccelerationStructureBuilder>;
    using HRayTracingPipeline = Handle<struct RayTracingPipeline>;

// -----------------------------------------------------------------------------

    enum class BufferFlags : u32
    {
        None,
        Addressable  = 1 << 0,
        DeviceLocal  = 1 << 1 | Addressable,
        Mapped       = 1 << 2,
    };
    NOVA_DECORATE_FLAG_ENUM(BufferFlags)

    enum class BufferUsage : u32
    {
        None,
        TransferSrc         = 1 << 0,
        TransferDst         = 1 << 1,
        Uniform             = 1 << 2,
        Storage             = 1 << 3,
        Index               = 1 << 4,
        Vertex              = 1 << 5,
        Indirect            = 1 << 6,
        ShaderBindingTable  = 1 << 7,
        AccelBuild          = 1 << 8,
        AccelStorage        = 1 << 9,
        DescriptorSamplers  = 1 << 10,
        DescriptorResources = 1 << 11,
    };
    NOVA_DECORATE_FLAG_ENUM(BufferUsage)

    enum class TextureFlags : u32
    {
        None,
        Array = 1 << 0,
        Mips  = 1 << 1,
    };
    NOVA_DECORATE_FLAG_ENUM(TextureFlags)

    enum class TextureUsage : u32
    {
        None,
        TransferSrc        = 1 << 0,
        TransferDst        = 1 << 1,
        Sampled            = 1 << 2,
        Storage            = 1 << 3,
        ColorAttach        = 1 << 4,
        DepthStencilAttach = 1 << 5,
    };
    NOVA_DECORATE_FLAG_ENUM(TextureUsage)

    enum class Format : u32
    {
        Undefined,

        RGBA8_SRGB,
        BGRA8_SRGB,

        R8_UNorm,
        RGBA8_UNorm,
        BGRA8_UNorm,

        R32_SFloat,
        RGB32_SFloat,
        RGBA16_SFloat,
        RGBA32_SFloat,

        R8_UInt,
        R16_UInt,
        R32_UInt,

        D24_UNorm,
        D32_SFloat,
        S8_D24_UNorm,
    };

    enum class IndexType : u32
    {
        U32,
        U16,
        U8,
    };

    enum class Filter : u32
    {
        Linear,
        Nearest,
    };

    enum class AddressMode : u32
    {
        Repeat,
        RepeatMirrored,
        Edge,
        Border,
    };

    enum class BorderColor : u32
    {
        TransparentBlack,
        Black,
        White,
    };

    enum class ShaderStage : u32
    {
        None,
        Vertex       = 1 << 0,
        TessControl  = 1 << 1,
        TessEval     = 1 << 2,
        Geometry     = 1 << 3,
        Fragment     = 1 << 4,
        Compute      = 1 << 5,
        RayGen       = 1 << 6,
        AnyHit       = 1 << 7,
        ClosestHit   = 1 << 8,
        Miss         = 1 << 9,
        Intersection = 1 << 10,
        Task         = 1 << 11,
        Mesh         = 1 << 12,
    };
    NOVA_DECORATE_FLAG_ENUM(ShaderStage)

    enum class PresentMode : u32
    {
        Immediate,
        Mailbox,
        Fifo,
        FifoRelaxed,
    };

    enum class BindPoint : u32
    {
        Graphics,
        Compute,
        RayTracing,
    };

    enum class AccelerationStructureType : u32
    {
        BottomLevel,
        TopLevel,
    };

    enum class AccelerationStructureFlags : u32
    {
        None,
        PreferFastTrace = 1 << 0,
        PreferFastBuild = 1 << 1,
        AllowDataAccess = 1 << 2,
        AllowCompaction = 1 << 3,
    };
    NOVA_DECORATE_FLAG_ENUM(AccelerationStructureFlags)

    enum class GeometryInstanceFlags : u32
    {
        None,
        TriangleCullClockwise        = 1 << 0,
        TriangleCullCounterClockwise = 1 << 1,
        InstanceForceOpaque          = 1 << 2,
    };
    NOVA_DECORATE_FLAG_ENUM(GeometryInstanceFlags)

    enum class DescriptorType : u8
    {
        SampledTexture,
        StorageTexture,
        Uniform,
        Storage,
        AccelerationStructure,
        Sampler,
    };

    struct DescriptorHandle
    {
        u32 id = 0;

        DescriptorHandle() = default;

        DescriptorHandle(u32 _id)
            : id(_id)
        {}

        u32 ToShaderUInt() const {
            return id;
        }
    };

    enum class CompareOp : u32
    {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always,
    };

    enum class CullMode : u32
    {
        None,
        Front = 1 << 0,
        Back  = 1 << 1,
        FrontAndBack = Front | Back,
    };
    NOVA_DECORATE_FLAG_ENUM(CullMode);

    enum class FrontFace : u32
    {
        CounterClockwise,
        Clockwise,
    };

    enum class PolygonMode : u32
    {
        Fill,
        Line,
        Point,
    };

    enum class Topology : u32
    {
        Points,
        Lines,
        LineStrip,
        Triangles,
        TriangleStrip,
        TriangleFan,
        Patches,
    };

    enum class CommandListType
    {
        Primary,
        Secondary,
    };

    enum class QueueFlags : u32
    {
        None,
        Graphics = 1 << 0,
        Compute  = 1 << 1,
        Transfer = 1 << 2,
    };
    NOVA_DECORATE_FLAG_ENUM(QueueFlags)

    enum class PipelineStage : u32
    {
        None,
        Transfer   = 1 << 0,
        Graphics   = 1 << 1,
        RayTracing = 1 << 2,
        Compute    = 1 << 3,
        AccelBuild = 1 << 4,
        All        = Transfer | Graphics | RayTracing | Compute | AccelBuild,
    };
    NOVA_DECORATE_FLAG_ENUM(PipelineStage)

    enum class TextureLayout : u32
    {
        GeneralImage,
        ColorAttachment,
        DepthStencilAttachment,
        Sampled,
        Present,
    };

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
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
        Topology      topology    = Topology::Triangles;
        CullMode      cullMode    = CullMode::Back;
        FrontFace    frontFace    = FrontFace::CounterClockwise;
        PolygonMode   polyMode    = PolygonMode::Fill;
        f32          lineWidth    = 1.f;
        CompareOp depthCompare    = CompareOp::Greater;
        u32        blendEnable: 1 = false;
        u32        depthEnable: 1 = false;
        u32         depthWrite: 1 = true;
        u32       flipVertical: 1 = false;
    };

// -----------------------------------------------------------------------------

    struct HitShaderGroup
    {
        HShader   closestHitShader = {};
        HShader       anyHitShader = {};
        HShader intersectionShader = {};
    };

// -----------------------------------------------------------------------------

    struct ContextConfig
    {
        bool      debug = false;
        bool rayTracing = false;
    };

    struct Context : Handle<Context>
    {
        static Context Create(const struct ContextConfig&);
        void Destroy();

        void WaitIdle() const;
        const ContextConfig& GetConfig() const;
        Queue GetQueue(QueueFlags, u32 index) const;
    };

// -----------------------------------------------------------------------------

    struct Queue : Handle<Queue>
    {
        friend Context;

        void Submit(Span<HCommandList>, Span<HFence> waits, Span<HFence> signals) const;
        bool Acquire(Span<HSwapchain>, Span<HFence> signals) const;
        void Present(Span<HSwapchain>, Span<HFence> waits, bool hostWait = false) const;
    };

// -----------------------------------------------------------------------------

    struct Fence : Handle<Fence>
    {
        static Fence Create(HContext);
        void Destroy();

        void Wait(u64 waitValue = ~0ull) const;
        u64  Advance() const;
        void Signal(u64 signalValue = ~0ull) const;
        u64  GetPendingValue() const;
    };

// -----------------------------------------------------------------------------

    struct CommandPool : Handle<CommandPool>
    {
        static CommandPool Create(HContext, HQueue);
        void Destroy();

        CommandList Begin() const;
        void Reset() const;
    };

// -----------------------------------------------------------------------------

    struct CommandList : Handle<CommandList>
    {
        friend CommandPool;

        void Present(HSwapchain) const;

        void ResetGraphicsState() const;
        void SetViewports(Span<Rect2I> rects, bool copyToScissors = false) const;
        void SetScissors(Span<Rect2I> scissors) const;
        void SetPolygonState(Topology, PolygonMode, CullMode, FrontFace, f32 lineWidth) const;
        void SetDepthState(bool testEnable, bool writeEnable, CompareOp) const;
        void SetBlendState(Span<bool> blends) const;
        void BindShaders(Span<HShader>) const;

        void WriteStorageBuffer(DescriptorHeap, DescriptorHandle handle, HBuffer, u64 size = ~0ull, u64 offset = 0) const;
        void WriteUniformBuffer(DescriptorHeap, DescriptorHandle handle, HBuffer, u64 size = ~0ull, u64 offset = 0) const;
        void WriteSampledTexture(DescriptorHeap, DescriptorHandle handle, HTexture) const;
        void WriteSampler(DescriptorHeap, DescriptorHandle handle, HSampler) const;
        void WriteStorageTexture(DescriptorHeap, DescriptorHandle handle, HTexture) const;

        void PushConstants(u64 offset, u64 size, const void* data) const;
        template<class T>
        void PushConstants(const T& constants, u64 offset = 0) const
        {
            PushConstants(offset, sizeof(constants), &constants);
        }

        void Barrier(PipelineStage src, PipelineStage dst) const;

        void BeginRendering(Rect2D region, Span<HTexture> colorAttachments, HTexture depthAttachment = {}, HTexture stencilAttachment = {}) const;
        void EndRendering() const;
        void BindIndexBuffer(HBuffer, IndexType, u64 offset = {}) const;
        void ClearColor(u32 attachment, std::variant<Vec4, Vec4U, Vec4I> value, Vec2U size, Vec2I offset = {}) const;
        void ClearDepth(f32 depth, Vec2U size, Vec2I offset = {}) const;
        void ClearStencil(u32 value, Vec2U size, Vec2I offset = {}) const;

        void Draw(u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance) const;
        void DrawIndirect(HBuffer, u64 offset, u32 count, u32 stride) const;
        void DrawIndirectCount(HBuffer commands, u64 commandOffset, HBuffer count, u64 countOffset, u32 maxCount, u32 stride) const;

        void DrawIndexed(u32 indices, u32 instances, u32 firstIndex, u32 vertexOffset, u32 firstInstance) const;
        void DrawIndexedIndirect(HBuffer, u64 offset, u32 count, u32 stride) const;
        void DrawIndexedIndirectCount(HBuffer commands, u64 commandOffset, HBuffer count, u64 countOffset, u32 maxCount, u32 stride) const;

        void DrawMeshTasks(Vec3U groups) const;
        void DrawMeshTasksIndirect(HBuffer, u64 offset, u32 count, u32 stride) const;
        void DrawMeshTasksIndirectCount(HBuffer commands, u64 commandOffset, HBuffer count, u64 countOffset, u32 maxCount, u32 stride) const;

        void Dispatch(Vec3U groups) const;
        void DispatchIndirect(HBuffer buffer, u64 offset) const;

        void BindDescriptorHeap(BindPoint, HDescriptorHeap) const;
        void BindAccelerationStructure(BindPoint, HAccelerationStructure) const;

        void UpdateBuffer(HBuffer dst, const void* data, usz size, u64 dstOffset = 0) const;
        void CopyToBuffer(HBuffer dst, HBuffer src, u64 size, u64 dstOffset = 0, u64 srcOffset = 0) const;
        void Barrier(HBuffer, PipelineStage src, PipelineStage dst) const;

        void Transition(HTexture, VkImageLayout, VkPipelineStageFlags2) const;
        void Transition(HTexture, TextureLayout, PipelineStage) const;
        void ClearColor(HTexture, std::variant<Vec4, Vec4U, Vec4I> value) const;
        void CopyToTexture(HTexture dst, HBuffer src, u64 srcOffset = 0) const;
        void CopyFromTexture(HBuffer dst, HTexture src, Rect2D regino) const;
        void GenerateMips(HTexture) const;
        void BlitImage(HTexture dst, HTexture src, Filter) const;

        void BuildAccelerationStructure(HAccelerationStructureBuilder, HAccelerationStructure, HBuffer scratch) const;
        void CompactAccelerationStructure(HAccelerationStructure dst, HAccelerationStructure src) const;

        void TraceRays(HRayTracingPipeline, Vec3U extent, u32 genIndex) const;
    };

// -----------------------------------------------------------------------------

    struct Swapchain : Handle<Swapchain>
    {
        static Swapchain Create(HContext, void* window, TextureUsage, PresentMode);
        void Destroy();

        Texture GetCurrent() const;
        Vec2U GetExtent() const;
        Format GetFormat() const;
    };

// -----------------------------------------------------------------------------

    struct DescriptorHeap : Handle<DescriptorHeap>
    {
        static DescriptorHeap Create(HContext, u32 requestedDescriptorCount);
        void Destroy();

        u32 GetMaxDescriptorCount() const;

        void WriteStorageBuffer(DescriptorHandle handle, HBuffer, u64 size = ~0ull, u64 offset = 0) const;
        void WriteUniformBuffer(DescriptorHandle handle, HBuffer, u64 size = ~0ull, u64 offset = 0) const;
        void WriteSampledTexture(DescriptorHandle handle, HTexture) const;
        void WriteSampler(DescriptorHandle handle, HSampler) const;
        void WriteStorageTexture(DescriptorHandle handle, HTexture) const;
    };

// -----------------------------------------------------------------------------

    struct Shader : Handle<Shader>
    {
        static Shader Create(HContext, ShaderStage, std::string entry, Span<u32> bytecode);
        void Destroy();
    };

// -----------------------------------------------------------------------------

    struct Sampler : Handle<Sampler>
    {
        static Sampler Create(HContext, Filter, AddressMode, BorderColor, f32 anisotropy);
        void Destroy();
    };

// -----------------------------------------------------------------------------

    struct Texture : Handle<Texture>
    {
        static Texture Create(HContext, Vec3U size, TextureUsage, Format, TextureFlags = {});
        void Destroy();

        Vec3U GetExtent() const;
        Format GetFormat() const;

        // TODO: Handle row pitch, etc..
        void Set(Vec3I offset, Vec3U extent, const void* data) const;
        void Transition(TextureLayout layout) const;
    };

// -----------------------------------------------------------------------------

    struct Buffer : Handle<Buffer>
    {
        static Buffer Create(HContext, u64 size, BufferUsage, BufferFlags = {});
        void Destroy();

        void Resize(u64 size) const;
        u64 GetSize() const;
        b8* GetMapped() const;
        u64 GetAddress() const;

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

// -----------------------------------------------------------------------------

    struct AccelerationStructureBuilder : Handle<AccelerationStructureBuilder>
    {
        static AccelerationStructureBuilder Create(HContext);
        void Destroy();

        void SetInstances(u32 geometryIndex, u64 deviceAddress, u32 count) const;
        void SetTriangles(u32 geometryIndex,
            u64 vertexAddress, Format vertexFormat, u32 vertexStride, u32 maxVertex,
            u64 indexAddress, IndexType indexFormat, u32 triangleCount) const;

        void Prepare(AccelerationStructureType, AccelerationStructureFlags, u32 geometryCount, u32 firstGeometry = 0) const;

        u32 GetInstanceSize() const;
        void WriteInstance(void* bufferAddress, u32 index,
            HAccelerationStructure,
            const Mat4& transform,
            u32 customIndex, u8 mask,
            u32 sbtOffset, GeometryInstanceFlags flags) const;

        u64 GetBuildSize() const;
        u64 GetBuildScratchSize() const;
        u64 GetUpdateScratchSize() const;
        u64 GetCompactSize() const;
    };

// -----------------------------------------------------------------------------

    struct AccelerationStructure : Handle<AccelerationStructure>
    {
        static AccelerationStructure Create(HContext, u64 size, AccelerationStructureType, HBuffer = {}, u64 offset = 0);
        void Destroy();

        u64 GetAddress() const;
    };

// -----------------------------------------------------------------------------

    struct RayTracingPipeline : Handle<RayTracingPipeline>
    {
        static RayTracingPipeline Create(HContext);
        void Destroy();

        void Update(
            Span<HShader> rayGenShaders,
            Span<HShader> rayMissShaders,
            Span<struct HitShaderGroup> rayhitShaderGroups,
            Span<HShader> callableShaders) const;
    };

#undef NOVA_BEGIN_API_OBJECT
#undef NOVA_END_API_OBJECT
}
