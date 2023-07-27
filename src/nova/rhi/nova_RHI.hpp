#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Ref.hpp>
#include <nova/core/nova_Math.hpp>

// #define NOVA_NOISY_VULKAN_ALLOCATIONS

namespace nova
{
    inline std::atomic<u64> TimeSubmitting = 0;
    inline std::atomic<u64> TimeAdaptingFromAcquire = 0;
    inline std::atomic<u64> TimeAdaptingToPresent = 0;
    inline std::atomic<u64> TimePresenting = 0;
    inline std::atomic<u64> TimeSettingGraphicsState = 0;
    inline std::atomic<u64> MemoryAllocated = 0;

    inline
    void VkCall(VkResult res)
    {
        if (res != VK_SUCCESS)
            NOVA_THROW("Error: {}", int(res));
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

    enum class Buffer : u32 {};
    enum class CommandList : u32 {};
    enum class CommandPool : u32 {};
    enum class DescriptorSet : u32 {};
    enum class DescriptorSetLayout : u32 {};
    enum class Fence : u32 {};
    enum class PipelineLayout : u32 {};
    enum class Queue : u32 {};
    enum class CommandState : u32 {};
    enum class Sampler : u32 {};
    enum class Shader : u32 {};
    enum class Swapchain : u32 {};
    enum class Texture : u32 {};
    enum class AccelerationStructure : u32 {};
    enum class AccelerationStructureBuilder : u32 {};
    enum class RayTracingPipeline : u32 {};

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

    enum class DescriptorType : u32
    {
        SampledTexture,
        StorageTexture,
        Uniform,
        Storage,
        AccelerationStructure,
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

    enum class ShaderVarType
    {
        Mat2, Mat3, Mat4,
        Mat4x3, Mat3x4,
        Vec2, Vec3, Vec4,
        Vec2U, Vec3U, Vec4U,
        U32, U64,
        I32, I64,
        F32, F64,
    };

    inline constexpr
    u32 GetShaderVarTypeSize(ShaderVarType type)
    {
        switch (type)
        {
        break;case ShaderVarType::Mat2: return  4 * 4;
        break;case ShaderVarType::Mat3: return  9 * 4;
        break;case ShaderVarType::Mat4: return 16 * 4;

        break;case ShaderVarType::Mat4x3: return 12 * 4;
        break;case ShaderVarType::Mat3x4: return 12 * 4;

        break;case ShaderVarType::Vec2: return  2 * 4;
        break;case ShaderVarType::Vec3: return  3 * 4;
        break;case ShaderVarType::Vec4: return  4 * 4;

        break;case ShaderVarType::U32:
              case ShaderVarType::I32:
              case ShaderVarType::F32:
            return 4;

        break;case ShaderVarType::U64:
              case ShaderVarType::I64:
              case ShaderVarType::F64:
            return 8;
        }
        return 0;
    }

    inline constexpr
    u32 GetShaderVarTypeAlign(ShaderVarType type)
    {
        switch (type)
        {
        break;case ShaderVarType::U64:
            return 8;
        }
        return 4;
    }

    struct Member
    {
        std::string_view    name;
        ShaderVarType       type;
        std::optional<u32> count = std::nullopt;
    };

    namespace binding
    {
        struct SampledTexture
        {
            std::string         name;
            std::optional<u32> count;
        };

        struct StorageTexture
        {
            std::string         name;
            Format            format;
            std::optional<u32> count;
        };

        struct UniformBuffer
        {
            std::string            name;
            std::vector<Member> members;
            bool                dynamic = false;
            std::optional<u32>    count;
        };

        struct AccelerationStructure
        {
            std::string         name;
            std::optional<u32> count;
        };
    }

    using DescriptorBinding = std::variant<
        binding::SampledTexture,
        binding::StorageTexture,
        binding::AccelerationStructure,
        binding::UniformBuffer>;

    struct DescriptorSetBindingOffset
    {
        u32 buffer;
        u64 offset = {};
    };

    struct PushConstantRange
    {
        std::string              name;
        std::vector<Member> constants;
        // ShaderStage stages;
        // u32           size;
        u32         offset = 0;
    };

// -----------------------------------------------------------------------------

    enum class ShaderInputFlags
    {
        None,
        Flat,
        PerVertex,
    };
    NOVA_DECORATE_FLAG_ENUM(ShaderInputFlags)

    namespace shader
    {
        constexpr u32 ArrayCountUnsized = UINT32_MAX;
    }

    namespace shader
    {
        struct Structure
        {
            std::string     name;
            Span<Member> members;
        };

        struct Layout
        {
            PipelineLayout layout;
        };

        struct BufferReference
        {
            std::string name;
            std::optional<ShaderVarType> scalarType = std::nullopt;
        };

        struct Input
        {
            std::string       name;
            ShaderVarType     type;
            ShaderInputFlags flags = {};
        };

        struct Output
        {
            std::string   name;
            ShaderVarType type;
        };

        struct Fragment
        {
            std::string glsl;
        };

        struct ComputeKernel
        {
            Vec3U workGroups;
            std::string glsl;
        };

        struct Kernel
        {
            std::string glsl;
        };
    }

    using ShaderElement = std::variant<
        shader::Structure,
        shader::Layout,
        shader::BufferReference,
        shader::Input,
        shader::Output,
        shader::Fragment,
        shader::ComputeKernel,
        shader::Kernel>;

// -----------------------------------------------------------------------------

    struct HitShaderGroup
    {
        Shader   closestHitShader = {};
        Shader       anyHitShader = {};
        Shader intersectionShader = {};
    };

// -----------------------------------------------------------------------------

    enum class Backend
    {
        None,
        Vulkan,
    };

    struct ContextConfig
    {
        Backend        backend = {};
        bool             debug = false;
        bool       meshShaders = false;
        bool        rayTracing = false;
        bool descriptorBuffers = false;
    };

    struct Context : RefCounted
    {
        static Ref<Context> Create(const ContextConfig& config);

    public:
        virtual ~Context() {}

        virtual void WaitIdle() = 0;
        virtual const ContextConfig& GetConfig() = 0;

// -----------------------------------------------------------------------------

        virtual bool IsValid(Buffer) = 0;
        virtual bool IsValid(CommandList) = 0;
        virtual bool IsValid(CommandPool) = 0;
        virtual bool IsValid(DescriptorSet) = 0;
        virtual bool IsValid(DescriptorSetLayout) = 0;
        virtual bool IsValid(Fence) = 0;
        virtual bool IsValid(PipelineLayout) = 0;
        virtual bool IsValid(Queue) = 0;
        virtual bool IsValid(CommandState) = 0;
        virtual bool IsValid(Sampler) = 0;
        virtual bool IsValid(Shader) = 0;
        virtual bool IsValid(Swapchain) = 0;
        virtual bool IsValid(Texture) = 0;
        virtual bool IsValid(AccelerationStructure) = 0;
        virtual bool IsValid(AccelerationStructureBuilder) = 0;
        virtual bool IsValid(RayTracingPipeline) = 0;

// -----------------------------------------------------------------------------
//                                 Queue
// -----------------------------------------------------------------------------

        virtual Queue Queue_Get(QueueFlags flags, u32 index) = 0;
        virtual void  Queue_Submit(Queue, Span<CommandList> commandLists, Span<Fence> waits, Span<Fence> signals) = 0;
        virtual bool  Queue_Acquire(Queue, Span<Swapchain> swapchains, Span<Fence> signals) = 0;
        virtual void  Queue_Present(Queue, Span<Swapchain> swapchains, Span<Fence> waits, bool hostWait = false) = 0;

// -----------------------------------------------------------------------------
//                                 Fence
// -----------------------------------------------------------------------------

        virtual Fence Fence_Create() = 0;
        virtual void  Fence_Destroy(Fence) = 0;
        virtual void  Fence_Wait(Fence, u64 waitValue = 0ull) = 0;
        virtual u64   Fence_Advance(Fence) = 0;
        virtual void  Fence_Signal(Fence, u64 signalValue = 0ull) = 0;
        virtual u64   Fence_GetPendingValue(Fence) = 0;

// -----------------------------------------------------------------------------
//                                Commands
// -----------------------------------------------------------------------------

        virtual CommandPool Commands_CreatePool(Queue queue) = 0;
        virtual void        Commands_DestroyPool(CommandPool) = 0;
        virtual CommandList Commands_Begin(CommandPool pool, CommandState state) = 0;
        virtual void        Commands_Reset(CommandPool pool) = 0;

        virtual CommandState Commands_CreateState() = 0;
        virtual void         Commands_DestroyState(CommandState) = 0;
        virtual void         Commands_SetState(CommandState state, Texture texture,
            VkImageLayout layout, VkPipelineStageFlags2 stages, VkAccessFlags2 access) = 0;

// -----------------------------------------------------------------------------
//                                 Swapchain
// -----------------------------------------------------------------------------

        virtual Swapchain Swapchain_Create(void* window, TextureUsage usage, PresentMode presentMode) = 0;
        virtual void      Swapchain_Destroy(Swapchain) = 0;
        virtual Texture   Swapchain_GetCurrent(Swapchain) = 0;
        virtual Vec2U     Swapchain_GetExtent(Swapchain) = 0;
        virtual Format    Swapchain_GetFormat(Swapchain) = 0;

        virtual void Cmd_Present(CommandList, Swapchain swapchain) = 0;

// -----------------------------------------------------------------------------
//                                  Shader
// -----------------------------------------------------------------------------

        virtual Shader Shader_Create(ShaderStage stage, const std::string& filename, const std::string& sourceCode) = 0;
        virtual Shader Shader_Create(ShaderStage stage, Span<ShaderElement> elements) = 0;
        virtual void   Shader_Destroy(Shader) = 0;

// -----------------------------------------------------------------------------
//                             Pipeline Layout
// -----------------------------------------------------------------------------

        virtual PipelineLayout Pipelines_CreateLayout(Span<PushConstantRange> pushConstantRanges, Span<DescriptorSetLayout> descriptorSetLayouts, BindPoint bindPoint) = 0;
        virtual void           Pipelines_DestroyLayout(PipelineLayout) = 0;

        virtual void Cmd_SetGraphicsState(CommandList, PipelineLayout layout, Span<Shader> shaders, const PipelineState& state) = 0;
        virtual void Cmd_PushConstants(CommandList, PipelineLayout layout, u64 offset, u64 size, const void* data) = 0;

// -----------------------------------------------------------------------------
//                                Commands
// -----------------------------------------------------------------------------

        virtual void Cmd_Barrier(CommandList, PipelineStage src, PipelineStage dst) = 0;

// -----------------------------------------------------------------------------
//                                Drawing
// -----------------------------------------------------------------------------

        virtual void Cmd_BeginRendering(CommandList cmd, Rect2D region, Span<Texture> colorAttachments, Texture depthAttachment = {}, Texture stencilAttachment = {}) = 0;
        virtual void Cmd_EndRendering(CommandList cmd) = 0;
        virtual void Cmd_Draw(CommandList cmd, u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance) = 0;
        virtual void Cmd_DrawIndexed(CommandList cmd, u32 indices, u32 instances, u32 firstIndex, u32 vertexOffset, u32 firstInstance) = 0;
        virtual void Cmd_BindIndexBuffer(CommandList cmd, Buffer buffer, IndexType indexType, u64 offset = 0) = 0;
        virtual void Cmd_ClearColor(CommandList cmd, u32 attachment, Vec4 color, Vec2U size, Vec2I offset = {}) = 0;
        virtual void Cmd_ClearDepth(CommandList cmd, f32 depth, Vec2U size, Vec2I offset = {}) = 0;
        virtual void Cmd_ClearStencil(CommandList cmd, u32 value, Vec2U size, Vec2I offset = {}) = 0;

// -----------------------------------------------------------------------------
//                                 Compute
// -----------------------------------------------------------------------------

        virtual void Cmd_SetComputeState(CommandList, PipelineLayout layout, Shader shader) = 0;
        virtual void Cmd_Dispatch(CommandList, Vec3U groups) = 0;

// -----------------------------------------------------------------------------
//                               Descriptors
// -----------------------------------------------------------------------------

        virtual DescriptorSetLayout Descriptors_CreateSetLayout(Span<DescriptorBinding> bindings, bool pushDescriptors = false) = 0;
        virtual void                Descriptors_DestroySetLayout(DescriptorSetLayout) = 0;

        virtual DescriptorSet       Descriptors_AllocateSet(DescriptorSetLayout layout, u64 customSize = 0) = 0;
        virtual void                Descriptors_FreeSet(DescriptorSet) = 0;
        virtual void                Descriptors_WriteSampledTexture(DescriptorSet set, u32 binding, Texture texture, Sampler sampler, u32 arrayIndex = 0) = 0;
        virtual void                Descriptors_WriteUniformBuffer(DescriptorSet set, u32 binding, Buffer buffer, u32 arrayIndex = 0) = 0;

        virtual void Cmd_BindDescriptorSets(CommandList cmd, PipelineLayout pipelineLayout, u32 firstSet, Span<DescriptorSet> sets) = 0;

        virtual void Cmd_PushStorageTexture(CommandList cmd, PipelineLayout layout, u32 setIndex, u32 binding, Texture texture, u32 arrayIndex = 0) = 0;
        virtual void Cmd_PushAccelerationStructure(CommandList cmd, PipelineLayout layout, u32 setIndex, u32 binding, AccelerationStructure accelerationStructure, u32 arrayIndex = 0) = 0;

// -----------------------------------------------------------------------------
//                                 Buffer
// -----------------------------------------------------------------------------

        virtual Buffer Buffer_Create(u64 size, BufferUsage usage, BufferFlags flags = {}) = 0;
        virtual void   Buffer_Destroy(Buffer) = 0;
        virtual void   Buffer_Resize(Buffer, u64 size) = 0;
        virtual u64    Buffer_GetSize(Buffer) = 0;
        virtual b8*    Buffer_GetMapped(Buffer) = 0;
        virtual u64    Buffer_GetAddress(Buffer) = 0;
        virtual void*  BufferImpl_Get(Buffer, u64 index, u64 offset, usz stride) = 0;
        virtual void   BufferImpl_Set(Buffer, const void* data, usz count, u64 index, u64 offset, usz stride) = 0;

        template<typename T>
        T& Buffer_Get(Buffer buffer, u64 index, u64 offset = 0)
        {
            constexpr auto Stride = AlignUpPower2(sizeof(T), alignof(T));
            return *reinterpret_cast<T*>(BufferImpl_Get(buffer ,index, offset, Stride));
        }
        template<typename T>
        void Buffer_Set(Buffer buffer, Span<T> elements, u64 index = 0, u64 offset = 0)
        {
            constexpr auto Stride = AlignUpPower2(sizeof(T), alignof(T));
            BufferImpl_Set(buffer, elements.data(), elements.size(), index, offset, Stride);
        }

        virtual void Cmd_UpdateBuffer(CommandList, Buffer dst, const void* pData, usz size, u64 dstOffset = 0) = 0;
        virtual void Cmd_CopyToBuffer(CommandList, Buffer dst, Buffer src, u64 size, u64 dstOffset = 0, u64 srcOffset = 0) = 0;
        virtual void Cmd_Barrier(CommandList, Buffer buffer, PipelineStage src, PipelineStage dst) = 0;

// -----------------------------------------------------------------------------
//                                 Texture
// -----------------------------------------------------------------------------

        virtual Sampler Sampler_Create(Filter filter, AddressMode addressMode, BorderColor color, f32 anisotropy = 0.f) = 0;
        virtual void    Sampler_Destroy(Sampler) = 0;

        virtual Texture Texture_Create(Vec3U size, TextureUsage usage, Format format, TextureFlags flags = {}) = 0;
        virtual void    Texture_Destroy(Texture) = 0;
        virtual Vec3U   Texture_GetExtent(Texture) = 0;
        virtual Format  Texture_GetFormat(Texture) = 0;

        // virtual void Cmd_Transition(CommandList, Texture texture, VkImageLayout newLayout, VkPipelineStageFlags2 newStages, VkAccessFlags2 newAccess) = 0;
        virtual void Cmd_Transition(CommandList, Texture texture, TextureLayout layout, PipelineStage stage) = 0;
        virtual void Cmd_Clear(CommandList, Texture texture, Vec4 color) = 0;
        virtual void Cmd_CopyToTexture(CommandList, Texture dst, Buffer src, u64 srcOffset = 0) = 0;
        virtual void Cmd_CopyFromTexture(CommandList, Buffer dst, Texture src, Rect2D region) = 0;
        virtual void Cmd_GenerateMips(CommandList, Texture texture) = 0;
        virtual void Cmd_BlitImage(CommandList, Texture dst, Texture src, Filter filter) = 0;

// -----------------------------------------------------------------------------
//                          Acceleration Structures
// -----------------------------------------------------------------------------

        virtual AccelerationStructureBuilder AccelerationStructures_CreateBuilder() = 0;
        virtual void                         AccelerationStructures_DestroyBuilder(AccelerationStructureBuilder) = 0;

        virtual void AccelerationStructures_SetInstances(AccelerationStructureBuilder, u32 geometryIndex, u64 deviceAddress, u32 count) = 0;
        virtual void AccelerationStructures_SetTriangles(AccelerationStructureBuilder, u32 geometryIndex,
            u64 vertexAddress, Format vertexFormat, u32 vertexStride, u32 maxVertex,
            u64 indexAddress, IndexType indexFormat, u32 triangleCount) = 0;

        virtual void AccelerationStructures_Prepare(AccelerationStructureBuilder builder, AccelerationStructureType type, AccelerationStructureFlags flags,
            u32 geometryCount, u32 firstGeometry = 0u) = 0;

        virtual u32  AccelerationStructures_GetInstanceSize() = 0;
        virtual void AccelerationStructures_WriteInstance(
            void* bufferAddress, u32 index,
            AccelerationStructure structure,
            const Mat4& matrix,
            u32 customIndex, u8 mask,
            u32 sbtOffset, GeometryInstanceFlags flags) = 0;

        virtual u64 AccelerationStructures_GetBuildSize(AccelerationStructureBuilder) = 0;
        virtual u64 AccelerationStructures_GetBuildScratchSize(AccelerationStructureBuilder) = 0;
        virtual u64 AccelerationStructures_GetUpdateScratchSize(AccelerationStructureBuilder) = 0;
        virtual u64 AccelerationStructures_GetCompactSize(AccelerationStructureBuilder) = 0;

        virtual AccelerationStructure AccelerationStructures_Create(u64 size, AccelerationStructureType type, Buffer buffer = {}, u64 offset = {}) = 0;
        virtual void                  AccelerationStructures_Destroy(AccelerationStructure) = 0;
        virtual u64                   AccelerationStructures_GetAddress(AccelerationStructure) = 0;

        virtual void Cmd_BuildAccelerationStructure(CommandList, AccelerationStructureBuilder builder, AccelerationStructure structure, Buffer scratch) = 0;
        virtual void Cmd_CompactAccelerationStructure(CommandList, AccelerationStructure dst, AccelerationStructure src) = 0;

// -----------------------------------------------------------------------------
//                          Acceleration Structures
// -----------------------------------------------------------------------------

        virtual RayTracingPipeline RayTracing_CreatePipeline() = 0;
        virtual void               RayTracing_DestroyPipeline(RayTracingPipeline) = 0;
        virtual void               RayTracing_UpdatePipeline(RayTracingPipeline,
            PipelineLayout layout,
            Span<Shader> rayGenShaders,
            Span<Shader> rayMissShaders,
            Span<HitShaderGroup> rayHitShaderGroup,
            Span<Shader> callableShaders) = 0;

        virtual void Cmd_TraceRays(CommandList, RayTracingPipeline pipeline, Vec3U extent, u32 genIndex) = 0;
    };
}
