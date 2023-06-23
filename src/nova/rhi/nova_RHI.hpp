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

    enum class ShaderVarType
    {
        Mat2, Mat3, Mat4,
        Vec2, Vec3, Vec4,
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

    struct ContextConfig
    {
        bool debug = false;
        bool rayTracing = false;
        bool descriptorBuffers = false;
    };

    struct Context
    {
        virtual ~Context() {}

        virtual void WaitIdle() = 0;
        virtual const ContextConfig& GetConfig() = 0;

// -----------------------------------------------------------------------------
//                                 Queue
// -----------------------------------------------------------------------------

        virtual Queue Queue_Get(QueueFlags flags) = 0;
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
//                                Drawing
// -----------------------------------------------------------------------------

        virtual void Cmd_BeginRendering(CommandList cmd, Span<Texture> colorAttachments, Texture depthAttachment = {}, Texture stencilAttachment = {}) = 0;
        virtual void Cmd_EndRendering(CommandList cmd) = 0;
        virtual void Cmd_Draw(CommandList cmd, u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance) = 0;
        virtual void Cmd_DrawIndexed(CommandList cmd, u32 indices, u32 instances, u32 firstIndex, u32 vertexOffset, u32 firstInstance) = 0;
        virtual void Cmd_BindIndexBuffer(CommandList cmd, Buffer buffer, IndexType indexType, u64 offset = 0) = 0;
        virtual void Cmd_ClearColor(CommandList cmd, u32 attachment, Vec4 color, Vec2U size, Vec2I offset = {}) = 0;
        virtual void Cmd_ClearDepth(CommandList cmd, f32 depth, Vec2U size, Vec2I offset = {}) = 0;
        virtual void Cmd_ClearStencil(CommandList cmd, u32 value, Vec2U size, Vec2I offset = {}) = 0;

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

        template<class T>
        T& Buffer_Get(Buffer buffer, u64 index, u64 offset = 0)
        {
            constexpr auto Stride = AlignUpPower2(sizeof(T), alignof(T));
            return *reinterpret_cast<T*>(BufferImpl_Get(buffer ,index, offset, Stride));
        }
        template<class T>
        void Buffer_Set(Buffer buffer, Span<T> elements, u64 index = 0, u64 offset = 0)
        {
            constexpr auto Stride = AlignUpPower2(sizeof(T), alignof(T));
            BufferImpl_Set(buffer, elements.data(), elements.size(), index, offset, Stride);
        }

        virtual void Cmd_UpdateBuffer(CommandList, Buffer dst, const void* pData, usz size, u64 dstOffset = 0) = 0;
        virtual void Cmd_CopyToBuffer(CommandList, Buffer dst, Buffer src, u64 size, u64 dstOffset = 0, u64 srcOffset = 0) = 0;

// -----------------------------------------------------------------------------
//                                 Texture
// -----------------------------------------------------------------------------

        virtual Sampler Sampler_Create(Filter filter, AddressMode addressMode, BorderColor color, f32 anisotropy = 0.f) = 0;
        virtual void    Sampler_Destroy(Sampler) = 0;

        virtual Texture Texture_Create(Vec3U size, TextureUsage usage, Format format, TextureFlags flags = {}) = 0;
        virtual void    Texture_Destroy(Texture) = 0;
        virtual Vec3U   Texture_GetExtent(Texture) = 0;
        virtual Format  Texture_GetFormat(Texture) = 0;

        virtual void Cmd_Transition(CommandList, Texture texture, VkImageLayout newLayout, VkPipelineStageFlags2 newStages, VkAccessFlags2 newAccess) = 0;
        virtual void Cmd_Clear(CommandList, Texture texture, Vec4 color) = 0;
        virtual void Cmd_CopyToTexture(CommandList, Texture dst, Buffer src, u64 srcOffset = 0) = 0;
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

        virtual AccelerationStructure AccelerationStructures_Create(u64 size, AccelerationStructureType type) = 0;
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
