#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Ref.hpp>
#include <nova/core/nova_Math.hpp>
#include <nova/core/nova_Containers.hpp>
#include <nova/core/nova_Flags.hpp>

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
    using HFence = Handle<struct Fence>;
    using HQueue = Handle<struct Queue>;
    using HSampler = Handle<struct Sampler>;
    using HShader = Handle<struct Shader>;
    using HSwapchain = Handle<struct Swapchain>;
    using HImage = Handle<struct Image>;
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

    enum class ImageFlags : u32
    {
        None,
        Array = 1 << 0,
        Mips  = 1 << 1,
    };
    NOVA_DECORATE_FLAG_ENUM(ImageFlags)

    enum class ImageUsage : u32
    {
        None,
        TransferSrc        = 1 << 0,
        TransferDst        = 1 << 1,
        Sampled            = 1 << 2,
        Storage            = 1 << 3,
        ColorAttach        = 1 << 4,
        DepthStencilAttach = 1 << 5,
    };
    NOVA_DECORATE_FLAG_ENUM(ImageUsage)

    enum class Format : u32
    {
        Undefined,

        RGBA8_SRGB,
        BGRA8_SRGB,

        R8_UNorm,
        RG8_UNorm,
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

        BC1A_SRGB,
        BC1A_UNorm,
        BC1_SRGB,
        BC1_Unorm,
        BC2_SRGB,
        BC2_UNorm,
        BC3_SRGB,
        BC3_UNorm,
        BC4_UNorm,
        BC4_SNorm,
        BC5_UNorm,
        BC5_SNorm,
        BC6_UFloat,
        BC6_SFloat,
        BC7_SRGB,
        BC7_Unorm,

        _count,
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
        Callable     = 1 << 11,
        Task         = 1 << 12,
        Mesh         = 1 << 13,
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
        SampledImage,
        StorageImage,
        Uniform,
        Storage,
        AccelerationStructure,
        Sampler,
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

    enum class ImageLayout : u32
    {
        GeneralImage,
        ColorAttachment,
        DepthStencilAttachment,
        Sampled,
        Present,
    };

    enum class PresentFlag : u32
    {
        None,

        HostWaitOnFences = 1 << 0,
    };
    NOVA_DECORATE_FLAG_ENUM(PresentFlag);

    enum class ShaderLang
    {
        Glsl,
        Hlsl
    };

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

    struct RenderingDescription
    {
        Span<Format> color_formats;
        Format        depth_format = {};
        Format      stencil_format = {};
        u32                subpass = 0;
    };

// -----------------------------------------------------------------------------

    struct HitShaderGroup
    {
        HShader   closesthit_shader = {};
        HShader       anyhit_shader = {};
        HShader intersection_shader = {};
    };

// -----------------------------------------------------------------------------

    struct ContextConfig
    {
        bool            debug = false;
        bool extra_validation = false;
        bool            trace = false;
        bool         api_dump = false;
        bool      ray_tracing = false;
        bool     mesh_shading = false;
        bool    compatibility = false;
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
        void Present(Span<HSwapchain>, Span<HFence> waits, PresentFlag flags = {}) const;
    };

// -----------------------------------------------------------------------------

    struct Fence : Handle<Fence>
    {
        static Fence Create(HContext);
        void Destroy();

        void Wait(u64 wait_value = ~0ull) const;
        u64  Advance() const;
        void Signal(u64 signal_value = ~0ull) const;
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
        void SetViewports(Span<Rect2I> rects, bool copy_to_scissors = false) const;
        void SetScissors(Span<Rect2I> scissors) const;
        void SetPolygonState(Topology, PolygonMode, f32 line_width) const;
        void SetCullState(CullMode cull_mode, FrontFace front_face) const;
        void SetDepthState(bool test_enable, bool write_enable, CompareOp) const;
        void SetBlendState(Span<bool> blends) const;
        void BindShaders(Span<HShader>) const;

        void PushConstants(RawByteView data, u64 offset = 0) const;

        void Barrier(PipelineStage src, PipelineStage dst) const;

        void BeginRendering(Rect2D region, Span<HImage> color_attachments, HImage depth_attachment = {}, HImage stencil_attachment = {}) const;
        void EndRendering() const;
        void BindIndexBuffer(HBuffer, IndexType, u64 offset = {}) const;
        void ClearColor(u32 attachment, std::variant<Vec4, Vec4U, Vec4I> value, Vec2U size, Vec2I offset = {}) const;
        void ClearDepth(f32 depth, Vec2U size, Vec2I offset = {}) const;
        void ClearStencil(u32 value, Vec2U size, Vec2I offset = {}) const;

        void Draw(u32 vertices, u32 instances, u32 first_vertex, u32 first_instance) const;
        void DrawIndirect(HBuffer, u64 offset, u32 count, u32 stride) const;
        void DrawIndirectCount(HBuffer commands, u64 command_offset, HBuffer count, u64 count_offset, u32 max_count, u32 stride) const;

        void DrawIndexed(u32 indices, u32 instances, u32 first_index, u32 vertex_offset, u32 first_instance) const;
        void DrawIndexedIndirect(HBuffer, u64 offset, u32 count, u32 stride) const;
        void DrawIndexedIndirectCount(HBuffer commands, u64 command_offset, HBuffer count, u64 count_offset, u32 max_count, u32 stride) const;

        void DrawMeshTasks(Vec3U groups) const;
        void DrawMeshTasksIndirect(HBuffer, u64 offset, u32 count, u32 stride) const;
        void DrawMeshTasksIndirectCount(HBuffer commands, u64 command_offset, HBuffer count, u64 count_offset, u32 max_count, u32 stride) const;

        void Dispatch(Vec3U groups) const;
        void DispatchIndirect(HBuffer buffer, u64 offset) const;

        void UpdateBuffer(HBuffer dst, const void* data, usz size, u64 dst_offset = 0) const;
        void CopyToBuffer(HBuffer dst, HBuffer src, u64 size, u64 dst_offset = 0, u64 src_offset = 0) const;
        void Barrier(HBuffer, PipelineStage src, PipelineStage dst) const;

        void Transition(HImage, ImageLayout, PipelineStage) const;
        void ClearColor(HImage, std::variant<Vec4, Vec4U, Vec4I> value) const;
        void CopyToImage(HImage dst, HBuffer src, u64 src_offset = 0) const;
        void CopyFromImage(HBuffer dst, HImage src, Rect2D region) const;
        void GenerateMips(HImage) const;
        void BlitImage(HImage dst, HImage src, Filter) const;

        void BuildAccelerationStructure(HAccelerationStructureBuilder, HAccelerationStructure, HBuffer scratch) const;
        void CompactAccelerationStructure(HAccelerationStructure dst, HAccelerationStructure src) const;

        void TraceRays(HRayTracingPipeline, Vec3U extent, u64 hit_shader_address = 0, u32 hit_shader_count = 0) const;
    };

// -----------------------------------------------------------------------------

    struct Swapchain : Handle<Swapchain>
    {
        static Swapchain Create(HContext, void* window, ImageUsage, PresentMode);
        void Destroy();

        Image GetCurrent() const;
        Vec2U GetExtent() const;
        Format GetFormat() const;
    };

// -----------------------------------------------------------------------------

    struct Shader : Handle<Shader>
    {
        static Shader Create(HContext, ShaderLang, ShaderStage, std::string entry,
            std::string_view filename, Span<std::string_view> fragments = {});

        void Destroy();
    };

// -----------------------------------------------------------------------------

    struct Sampler : Handle<Sampler>
    {
        static Sampler Create(HContext, Filter, AddressMode, BorderColor, f32 anisotropy);
        void Destroy();

        u32 GetDescriptor() const;
    };

// -----------------------------------------------------------------------------

    struct Image : Handle<Image>
    {
        static Image Create(HContext, Vec3U size, ImageUsage, Format, ImageFlags = {});
        void Destroy();

        Vec3U GetExtent() const;
        Format GetFormat() const;

        // TODO: Handle row pitch, etc..
        void Set(Vec3I offset, Vec3U extent, const void* data) const;
        void Transition(ImageLayout layout) const;

        u32 GetDescriptor() const;
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

        void SetInstances(u32 geometry_index, u64 device_address, u32 count) const;
        void SetTriangles(u32 geometry_index,
            u64 vertex_address, Format vertex_format, u32 vertex_stride, u32 max_vertex,
            u64 index_address, IndexType index_format, u32 triangle_count) const;

        void Prepare(AccelerationStructureType, AccelerationStructureFlags, u32 geometry_count, u32 first_geometry = 0) const;

        u32 GetInstanceSize() const;
        void WriteInstance(void* buffer_address, u32 index,
            HAccelerationStructure,
            const Mat4& transform,
            u32 custom_index, u8 mask,
            u32 sbt_offset, GeometryInstanceFlags flags) const;

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
            HShader                            raygen_shader,
            Span<HShader>                    raymiss_shaders,
            Span<struct HitShaderGroup> rayhit_shader_groups,
            Span<HShader>                   callable_shaders) const;

        u64 GetTableSize(u32 handles) const;
        u64 GetHandleSize() const;
        u64 GetHandleStride() const;
        u64 GetHandleGroupAlign() const;
        HBuffer GetHandles() const;
        void WriteHandle(void* buffer_address, u32 index, u32 group_index);
    };
}
