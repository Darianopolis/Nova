#pragma once

#include <nova/core/nova_Core.hpp>

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
    enum class Surface : u32 {};
    enum class Swapchain : u32 {};
    enum class Texture : u32 {};
    enum class AccelerationStructure : u32 {};
    enum class AccelerationStructureBuilder : u32 {};
    enum class RayTracingPipeline : u32 {};
    enum class GraphicsPipeline : u32 {};
    enum class PipelineCache : u32 {};

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
        RGBA8_SRGB = VK_FORMAT_R8G8B8A8_SRGB,

        RGBA16F = VK_FORMAT_R16G16B16A16_SFLOAT,
        RGBA32F = VK_FORMAT_R32G32B32A32_SFLOAT,

        BGRA8U = VK_FORMAT_B8G8R8A8_UNORM,
        BGRA8_SRGB = VK_FORMAT_B8G8R8A8_SRGB,

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

    struct ContextConfig
    {
        bool debug = false;
        bool rayTracing = false;
        bool shaderObjects = false;
        bool descriptorBuffers = false;
    };

    struct Context
    {
        virtual ~Context() {}

// -----------------------------------------------------------------------------
//                                 Queue
// -----------------------------------------------------------------------------

        virtual void Queue_Submit(Span<CommandList> commandLists, Span<Fence> signals) = 0;
        virtual void Queue_Acquire(Span<Swapchain> swapchains, Span<Fence> signals) = 0;
        virtual void Queue_Present(Span<Swapchain> swapchains, Span<Fence> waits, bool hostWait = false) = 0;

// -----------------------------------------------------------------------------
//                                 Fence
// -----------------------------------------------------------------------------

        // virtual Fence Fence_Create();
        // virtual void  Destroy(Fence);
        // virtual void  Fence_Wait(Fence, u64 waitValue = 0ull);
        // virtual u64   Fence_Advance(Fence);
        // virtual void  Fence_Signal(Fence, u64 signalValue = 0ull);
        // virtual u64   Fence_GetPendingValue(Fence);

// -----------------------------------------------------------------------------
//                                 Swapchain
// -----------------------------------------------------------------------------

        virtual Swapchain Swapchain_Create(void* window, TextureUsage usage, PresentMode presentMode) = 0;
        virtual void      Destroy(Swapchain) = 0;
        virtual Texture   Swapchain_GetCurrent(Swapchain) = 0;
        virtual Vec2U     Swapchain_GetExtent(Swapchain) = 0;
        virtual Format    Swapchain_GetFormat(Swapchain) = 0;

// -----------------------------------------------------------------------------
//                                 Command
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                 Buffer
// -----------------------------------------------------------------------------

        virtual Buffer Buffer_Create(u64 size, BufferUsage usage, BufferFlags flags = {}) = 0;
        virtual void   Destroy(Buffer) = 0;
        virtual void   Buffer_Resize(Buffer, u64 size) = 0;
        virtual u64    Buffer_GetSize(Buffer) = 0;
        virtual b8*    Buffer_GetMapped(Buffer) = 0;
        virtual u64    Buffer_GetAddress(Buffer) = 0;
        virtual void*  BufferImpl_Get(Buffer, u64 index, u64 offset, usz stride) = 0;
        virtual void   BufferImpl_Set(Buffer, const void* data, usz count, u64 index, u64 offset, usz stride) = 0;

        template<class T>
        T& Buffer_Get(u64 index, u64 offset = 0) const noexcept
        {
            constexpr auto Stride = AlignUpPower2(sizeof(T), alignof(T));
            return *reinterpret_cast<T*>(BufferImpl_Get(index, offset, Stride));
        }
        template<class T>
        void Buffer_Set(Span<T> elements, u64 index = 0, u64 offset = 0) const noexcept
        {
            constexpr auto Stride = AlignUpPower2(sizeof(T), alignof(T));
            BufferImpl_Set(elements.data(), elements.size(), index, offset, Stride);
        }
    };
}
