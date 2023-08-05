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

    template<typename T, typename DerefT>
    concept Dereferenceable = requires(const T& t)
    {
        { *t } -> std::same_as<DerefT&>;
    };

    template<typename T>
    class Handle 
    {
        T* value = {};

    public:
        Handle() noexcept = default;

        template<typename T2>
        requires Dereferenceable<T2, T>
        Handle(const T2& t)
            : value(&*t)
        {}

        Handle(T* ptr) noexcept
            : value(ptr)
        {}

        Handle(T& ptr) noexcept
            : value(&ptr)
        {}

        bool operator==(const Handle& other) const noexcept
        {
            return value == other.value;
        }

        T* operator->() const noexcept
        {
            return value;
        }

        operator bool() const noexcept
        {
            return value;
        }

        T* get() const noexcept
        {
            return value;
        }
    };
    
    struct Context;
    using HContext = Handle<Context>;

    struct Object
    {
        HContext context;
    
    public:
        Object(HContext _context)
            : context(_context)
        {}

        virtual ~Object() = 0;
    };

    inline
    Object::~Object() {}

    struct Buffer;
    struct CommandList;
    struct CommandPool;
    struct DescriptorSet;
    struct DescriptorSetLayout;
    struct Fence;
    struct PipelineLayout;
    struct Queue;
    struct CommandState;
    struct Sampler;
    struct Shader;
    struct Swapchain;
    struct Texture;
    struct AccelerationStructure;
    struct AccelerationStructureBuilder;
    struct RayTracingPipeline;

    using HBuffer = Handle<Buffer>;
    using HCommandList = Handle<CommandList>;
    using HCommandPool = Handle<CommandPool>;
    using HDescriptorSet = Handle<DescriptorSet>;
    using HDescriptorSetLayout = Handle<DescriptorSetLayout>;
    using HFence = Handle<Fence>;
    using HPipelineLayout = Handle<PipelineLayout>;
    using HQueue = Handle<Queue>;
    using HCommandState = Handle<CommandState>;
    using HSampler = Handle<Sampler>;
    using HShader = Handle<Shader>;
    using HSwapchain = Handle<Swapchain>;
    using HTexture = Handle<Texture>;
    using HAccelerationStructure = Handle<AccelerationStructure>;
    using HAccelerationStructureBuilder = Handle<AccelerationStructureBuilder>;
    using HRayTracingPipeline = Handle<RayTracingPipeline>;

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
}
