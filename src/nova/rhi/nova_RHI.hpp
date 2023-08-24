#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Ref.hpp>
#include <nova/core/nova_Math.hpp>

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

    template<typename T>
    class ImplHandle
    {
        void* impl = {};

    public:
        ImplHandle() = default;
        ImplHandle(T t): impl(t.impl) {}

        operator bool() const noexcept { return impl; }

             operator T() const noexcept { return T(static_cast<decltype(T::impl)>(impl)); }
           T operator()() const noexcept { return T(static_cast<decltype(T::impl)>(impl)); }
        auto operator->() const noexcept { return  (static_cast<decltype(T::impl)>(impl)) ; }
    };

#define NOVA_DECLARE_API_HANDLE(type) using H##type = ImplHandle<struct type>;
    
    NOVA_DECLARE_API_HANDLE(Context);
    NOVA_DECLARE_API_HANDLE(Buffer);
    NOVA_DECLARE_API_HANDLE(CommandList);
    NOVA_DECLARE_API_HANDLE(CommandPool);
    NOVA_DECLARE_API_HANDLE(DescriptorSet);
    NOVA_DECLARE_API_HANDLE(DescriptorSetLayout);
    NOVA_DECLARE_API_HANDLE(Fence);
    NOVA_DECLARE_API_HANDLE(PipelineLayout);
    NOVA_DECLARE_API_HANDLE(Queue);
    NOVA_DECLARE_API_HANDLE(CommandState);
    NOVA_DECLARE_API_HANDLE(Sampler);
    NOVA_DECLARE_API_HANDLE(Shader);
    NOVA_DECLARE_API_HANDLE(Swapchain);
    NOVA_DECLARE_API_HANDLE(Texture);
    NOVA_DECLARE_API_HANDLE(AccelerationStructure);
    NOVA_DECLARE_API_HANDLE(AccelerationStructureBuilder);
    NOVA_DECLARE_API_HANDLE(RayTracingPipeline);

#undef NOVA_DECLARE_API_HANDLE

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

    enum class ShaderVarType
    {
        Mat2, Mat3, Mat4,
        Mat4x3, Mat3x4,
        Vec2, Vec3, Vec4,
        Vec2U, Vec3U, Vec4U,
        U32, U64,
        I16, I32, I64,
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

        break;case ShaderVarType::I16:
            return 2;

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
    
// -----------------------------------------------------------------------------

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
    
// -----------------------------------------------------------------------------

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
        u32                    offset = 0;
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
            HPipelineLayout layout;
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
        HShader   closestHitShader = {};
        HShader       anyHitShader = {};
        HShader intersectionShader = {};
    };

// -----------------------------------------------------------------------------

    struct ContextConfig
    {
        bool             debug = false;
        bool       meshShaders = false;
        bool        rayTracing = false;
        bool descriptorBuffers = false;
    };

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

#define NOVA_BEGIN_API_OBJECT(type, ...) \
    struct type { \
        struct Impl; \
        Impl* impl = {}; \
        operator bool() const noexcept { return impl; } \
        Impl* operator->() const noexcept { return impl; }
#define NOVA_END_API_OBJECT() };
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(Context)
        static Context Create(const struct ContextConfig&);
        void Destroy();

        void WaitIdle() const;
        const ContextConfig& GetConfig() const;
        Queue GetQueue(QueueFlags, u32 index) const;
    NOVA_END_API_OBJECT()

// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(Queue)
        void Submit(Span<HCommandList>, Span<HFence> waits, Span<HFence> signals) const;
        bool Acquire(Span<HSwapchain>, Span<HFence> signals) const;
        void Present(Span<HSwapchain>, Span<HFence> waits, bool hostWait = false) const;
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(Fence)
        static Fence Create(HContext);
        void Destroy();

        void Wait(u64 waitValue = ~0ull) const;
        u64  Advance() const;
        void Signal(u64 signalValue = ~0ull) const;
        u64  GetPendingValue() const;
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(CommandPool)
        static CommandPool Create(HContext, HQueue);
        void Destroy();

        CommandList Begin(HCommandState) const;
        void Reset() const;
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(CommandState)
        static CommandState Create(HContext);
        void Destroy();

        void SetState(HTexture, VkImageLayout, VkPipelineStageFlags2, VkAccessFlags2) const;
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(CommandList)
        void Present(HSwapchain) const;

        void SetGraphicsState(HPipelineLayout, Span<HShader>, const PipelineState&) const;
        void PushConstants(HPipelineLayout, u64 offset, u64 size, const void* data) const;
        
        void Barrier(PipelineStage src, PipelineStage dst) const;
        
        void BeginRendering(Rect2D region, Span<HTexture> colorAttachments, HTexture depthAttachment = {}, HTexture stencilAttachment = {}) const;
        void EndRendering() const;
        void Draw(u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance) const;
        void DrawIndexed(u32 indices, u32 instances, u32 firstIndex, u32 vertexOffset, u32 firstInstance) const;
        void BindIndexBuffer(HBuffer, IndexType, u64 offset = {}) const;
        void ClearColor(u32 attachment, Vec4 color, Vec2U size, Vec2I offset = {}) const;
        void ClearDepth(f32 depth, Vec2U size, Vec2I offset = {}) const;
        void ClearStencil(u32 value, Vec2U size, Vec2I offset = {}) const;

        void SetComputeState(HPipelineLayout, HShader) const;
        void Dispatch(Vec3U groups) const;

        void BindDescriptorSets(HPipelineLayout, u32 firstSet, Span<HDescriptorSet>) const;
        
        void PushStorageTexture(HPipelineLayout, u32 setIndex, u32 binding, HTexture, u32 arrayIndex = 0) const;
        void PushAccelerationStructure(HPipelineLayout, u32 setIndex, u32 binding, HAccelerationStructure, u32 arrayIndex = 0) const;

        void UpdateBuffer(HBuffer dst, const void* data, usz size, u64 dstOffset = 0) const;
        void CopyToBuffer(HBuffer dst, HBuffer src, u64 size, u64 dstOffset = 0, u64 srcOffset = 0) const;
        void Barrier(HBuffer, PipelineStage src, PipelineStage dst) const;

        void Transition(HTexture, VkImageLayout, VkPipelineStageFlags2, VkAccessFlags2) const;
        void Transition(HTexture, TextureLayout, PipelineStage) const;
        void Clear(HTexture, Vec4 color) const;
        void CopyToTexture(HTexture dst, HBuffer src, u64 srcOffset = 0) const;
        void CopyFromTexture(HBuffer dst, HTexture src, Rect2D regino) const;
        void GenerateMips(HTexture) const;
        void BlitImage(HTexture dst, HTexture src, Filter) const;

        void BuildAccelerationStructure(HAccelerationStructureBuilder, HAccelerationStructure, HBuffer scratch) const;
        void CompactAccelerationStructure(HAccelerationStructure dst, HAccelerationStructure src) const;

        void TraceRays(HRayTracingPipeline, Vec3U extent, u32 genIndex) const;
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(Swapchain)
        static Swapchain Create(HContext, void* window, TextureUsage, PresentMode);
        void Destroy();

        Texture GetCurrent() const;
        Vec2U GetExtent() const;
        Format GetFormat() const;
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(PipelineLayout)
        static PipelineLayout Create(HContext, Span<PushConstantRange>, Span<HDescriptorSetLayout>, BindPoint);
        void Destroy();
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(DescriptorSetLayout)
        static DescriptorSetLayout Create(HContext, Span<DescriptorBinding>, bool pushDescriptors = false);
        void Destroy();
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(DescriptorSet)
        static DescriptorSet Create(HDescriptorSetLayout, u64 customSize = 0);
        void Destroy();

        void WriteSampledTexture(u32 binding, HTexture, HSampler, u32 arrayIndex = 0) const;
        void WriteUniformBuffer(u32 binding, HBuffer, u32 arrayIndex = 0) const;
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(Shader)
        static Shader Create(HContext, ShaderStage, const std::string& filename, const std::string& code);
        static Shader Create(HContext, ShaderStage, Span<ShaderElement> elements);
        void Destroy();
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(Sampler)
        static Sampler Create(HContext, Filter, AddressMode, BorderColor, f32 anisotropy);
        void Destroy();
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(Texture)
        static Texture Create(HContext, Vec3U size, TextureUsage, Format, TextureFlags = {});
        void Destroy();

        Vec3U GetExtent() const;
        Format GetFormat() const;
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(Buffer)
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
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(AccelerationStructureBuilder)
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
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(AccelerationStructure)
        static AccelerationStructure Create(HContext, u64 size, AccelerationStructureType, HBuffer = {}, u64 offset = 0);
        void Destroy();

        u64 GetAddress() const;
    NOVA_END_API_OBJECT()
    
// -----------------------------------------------------------------------------

    NOVA_BEGIN_API_OBJECT(RayTracingPipeline)
        static RayTracingPipeline Create(HContext);
        void Destroy();

        void Update(HPipelineLayout, 
            Span<HShader> rayGenShaders,
            Span<HShader> rayMissShaders,
            Span<struct HitShaderGroup> rayhitShaderGroups,
            Span<HShader> callableShaders) const;
    NOVA_END_API_OBJECT()

#undef NOVA_BEGIN_API_OBJECT
#undef NOVA_END_API_OBJECT
}
