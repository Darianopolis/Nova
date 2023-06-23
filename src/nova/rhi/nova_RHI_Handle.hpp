#include "nova_RHI.hpp"

namespace nova
{
    struct HQueue
    {
        Context* ctx = {};
        Queue  queue = {};

        operator Queue() { return queue; }

    public:
        HQueue(Context* ctx, QueueFlags flags)
            : ctx(ctx)
        {
            queue = ctx->Queue_Get(flags);
        }

        void Submit(Span<CommandList> commandLists, Span<Fence> waits, Span<Fence> signals)
        {
            ctx->Queue_Submit(queue, commandLists, waits, signals);
        }

        bool Acquire(Span<Swapchain> swapchains, Span<Fence> signals)
        {
            return ctx->Queue_Acquire(queue, swapchains, signals);
        }

        void Present(Span<Swapchain> swapchains, Span<Fence> waits, bool hostWait = false)
        {
            ctx->Queue_Present(queue, swapchains, waits, hostWait);
        }
    };

// -----------------------------------------------------------------------------
//                                 Fence
// -----------------------------------------------------------------------------

    struct HFence
    {
        Context* ctx = {};
        Fence  fence = {};

        operator Fence() { return fence; };

    public:
        HFence(Context* ctx)
            : ctx(ctx)
            , fence(ctx->Fence_Create())
        {}

        void Destroy()
        {
            ctx->Fence_Destroy(fence);
        }

        void Wait(u64 waitValue = 0ull)
        {
            ctx->Fence_Wait(fence, waitValue);
        }

        u64 Advance()
        {
            return ctx->Fence_Advance(fence);
        }

        void Signal(u64 signalValue = 0ull)
        {
            ctx->Fence_Signal(fence, signalValue);
        }

        u64 GetPendingValue()
        {
            return ctx->Fence_GetPendingValue(fence);
        }
    };

// -----------------------------------------------------------------------------
//                                Commands
// -----------------------------------------------------------------------------

    struct HCommandState
    {
        Context*       ctx = {};
        CommandState state = {};

        operator CommandState() { return state; }

    public:
        HCommandState(Context* ctx)
            : ctx(ctx)
            , state(ctx->Commands_CreateState())
        {}

        void SetState(CommandState state, Texture texture,
            VkImageLayout layout, VkPipelineStageFlags2 stages, VkAccessFlags2 access)
        {
            ctx->Commands_SetState(state, texture, layout, stages, access);
        }
    };

    struct HCommandList
    {
        Context*    ctx = {};
        CommandList cmd = {};

        operator CommandList() { return cmd; }

    public:

        // Textures

        void Transition(Texture texture, VkImageLayout newLayout, VkPipelineStageFlags2 newStages, VkAccessFlags2 newAccess)
        {
            ctx->Cmd_Transition(cmd, texture, newLayout, newStages, newAccess);
        }

        void Clear(Texture texture, Vec4 color)
        {
            ctx->Cmd_Clear(cmd, texture, color);
        }

        void CopyToTexture(Texture dst, Buffer src, u64 srcOffset = 0)
        {
            ctx->Cmd_CopyToTexture(cmd, dst, src, srcOffset);
        }

        void GenerateMips(Texture texture)
        {
            ctx->Cmd_GenerateMips(cmd, texture);
        }

        void BlitImage(Texture dst, Texture src, Filter filter)
        {
            ctx->Cmd_BlitImage(cmd, dst, src, filter);
        }

        // Swapchain

        void Present(Swapchain swapchain)
        {
            ctx->Cmd_Present(cmd, swapchain);
        }

        // Pipelines

        void SetGraphicsState(PipelineLayout layout, Span<Shader> shaders, const PipelineState& state)
        {
            ctx->Cmd_SetGraphicsState(cmd, layout, shaders, state);
        }

        void PushConstants(PipelineLayout layout, u64 offset, u64 size, const void* data)
        {
            ctx->Cmd_PushConstants(cmd, layout, offset, size, data);
        }

        // Drawing

        void BeginRendering(Span<Texture> colorAttachments, Texture depthAttachment = {}, Texture stencilAttachment = {})
        {
            ctx->Cmd_BeginRendering(cmd, colorAttachments, depthAttachment, stencilAttachment);
        }

        void EndRendering()
        {
            ctx->Cmd_EndRendering(cmd);
        }

        void Draw(u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance)
        {
            ctx->Cmd_Draw(cmd, vertices, instances, firstVertex, firstInstance);
        }

        void DrawIndexed(u32 indices, u32 instances, u32 firstIndex, u32 vertexOffset, u32 firstInstance)
        {
            ctx->Cmd_DrawIndexed(cmd, indices, instances, firstIndex, vertexOffset, firstInstance);
        }

        void BindIndexBuffer(Buffer buffer, IndexType indexType, u64 offset = 0)
        {
            ctx->Cmd_BindIndexBuffer(cmd, buffer, indexType, offset);
        }

        void ClearColor(u32 attachment, Vec4 color, Vec2U size, Vec2I offset = {})
        {
            ctx->Cmd_ClearColor(cmd, attachment, color, size, offset);
        }

        void ClearDepth(f32 depth, Vec2U size, Vec2I offset = {})
        {
            ctx->Cmd_ClearDepth(cmd, depth, size, offset);
        }

        void ClearStencil(u32 value, Vec2U size, Vec2I offset = {})
        {
            ctx->Cmd_ClearStencil(cmd, value, size, offset);
        }

        // Descriptors

        void BindDescriptorSets(PipelineLayout pipelineLayout, u32 firstSet, Span<DescriptorSet> sets)
        {
            ctx->Cmd_BindDescriptorSets(cmd, pipelineLayout, firstSet, sets);
        }

        void PushStorageTexture(PipelineLayout layout, u32 setIndex, u32 binding, Texture texture, u32 arrayIndex = 0)
        {
            ctx->Cmd_PushStorageTexture(cmd, layout, setIndex, binding, texture, arrayIndex);
        }

        void PushAccelerationStructure(PipelineLayout layout, u32 setIndex, u32 binding, AccelerationStructure accelerationStructure, u32 arrayIndex = 0)
        {
            ctx->Cmd_PushAccelerationStructure(cmd, layout, setIndex, binding, accelerationStructure, arrayIndex);
        }

        // Buffers

        void UpdateBuffer(Buffer dst, const void* pData, usz size, u64 dstOffset = 0)
        {
            ctx->Cmd_UpdateBuffer(cmd, dst, pData, size, dstOffset);
        }

        void CopyToBuffer(Buffer dst, Buffer src, u64 size, u64 dstOffset = 0, u64 srcOffset = 0)
        {
            ctx->Cmd_CopyToBuffer(cmd, dst, src, size, dstOffset, srcOffset);
        }

        // Acceleration Structures

        void BuildAccelerationStructure(AccelerationStructureBuilder builder, AccelerationStructure structure, Buffer scratch)
        {
            ctx->Cmd_BuildAccelerationStructure(cmd, builder, structure, scratch);
        }

        void CompactAccelerationStructure(AccelerationStructure dst, AccelerationStructure src)
        {
            ctx->Cmd_CompactAccelerationStructure(cmd, dst, src);
        }

        // Ray Tracing

        void TraceRays(RayTracingPipeline pipeline, Vec3U extent, u32 genIndex)
        {
            ctx->Cmd_TraceRays(cmd, pipeline, extent, genIndex);
        }
    };

    struct HCommandPool
    {
        Context*     ctx = {};
        CommandPool pool = {};

        operator CommandPool() { return pool; }

    public:
        HCommandPool(Context* ctx, Queue queue)
            : ctx(ctx)
            , pool(ctx->Commands_CreatePool(queue))
        {}

        void Destroy()
        {
            ctx->Commands_DestroyPool(pool);
        }

        HCommandList Begin(CommandState state)
        {
            return { ctx, ctx->Commands_Begin(pool, state) };
        }

        void Reset()
        {
            ctx->Commands_Reset(pool);
        }
    };

// -----------------------------------------------------------------------------
//                                 Texture
// -----------------------------------------------------------------------------

    struct HSampler
    {
        Context*    ctx = {};
        Sampler sampler = {};

        operator Sampler() { return sampler; }

    public:
        HSampler(Context* ctx, Filter filter, AddressMode addressMode, BorderColor color, f32 anisotropy = 0.f)
            : ctx(ctx)
            , sampler(ctx->Sampler_Create(filter, addressMode, color, anisotropy))
        {}

        void Destroy()
        {
            ctx->Sampler_Destroy(sampler);
        }
    };

    struct HTexture
    {
        Context*    ctx = {};
        Texture texture = {};

        operator Texture() { return texture; }

    public:
        HTexture() = default;

        HTexture(Context* ctx, Texture texture)
            : ctx(ctx)
            , texture(texture)
        {}

        HTexture(Context* ctx, Vec3U size, TextureUsage usage, Format format, TextureFlags flags = {})
            : ctx(ctx)
            , texture(ctx->Texture_Create(size, usage, format, flags))
        {}

        void Destroy()
        {
            ctx->Texture_Destroy(texture);
        }

        Vec3U GetExtent()
        {
            return ctx->Texture_GetExtent(texture);
        }

        Format GetFormat()
        {
            return ctx->Texture_GetFormat(texture);
        }
    };


// -----------------------------------------------------------------------------
//                                 Swapchain
// -----------------------------------------------------------------------------

    struct HSwapchain
    {
        Context*        ctx = {};
        Swapchain swapchain = {};

        operator Swapchain() { return swapchain; }

    public:
        HSwapchain(Context* ctx, void* window, TextureUsage usage, PresentMode presentMode)
            : ctx(ctx)
            , swapchain(ctx->Swapchain_Create(window, usage, presentMode))
        {}

        void Destroy()
        {
            ctx->Swapchain_Destroy(swapchain);
        }

        HTexture GetCurrent()
        {
            return { ctx, ctx->Swapchain_GetCurrent(swapchain) };
        }

        Vec2U GetExtent()
        {
            return ctx->Swapchain_GetExtent(swapchain);
        }

        Format GetFormat()
        {
            return ctx->Swapchain_GetFormat(swapchain);
        }
    };

// -----------------------------------------------------------------------------
//                                  Shader
// -----------------------------------------------------------------------------

    struct HShader
    {
        Context*  ctx = {};
        Shader shader = {};

        operator Shader() { return shader; }

    public:
        HShader(Context* ctx, ShaderStage stage, const std::string& filename, const std::string& sourceCode)
            : ctx(ctx)
            , shader(ctx->Shader_Create(stage, filename, sourceCode))
        {}

        HShader(Context* ctx, ShaderStage stage, Span<ShaderElement> elements)
            : ctx(ctx)
            , shader(ctx->Shader_Create(stage, elements))
        {}

        void Destroy()
        {
            ctx->Shader_Destroy(shader);
        }
    };

// -----------------------------------------------------------------------------
//                             Pipeline Layout
// -----------------------------------------------------------------------------

    struct HPipelineLayout
    {
        Context*                  ctx = {};
        PipelineLayout pipelineLayout = {};

        operator PipelineLayout() { return pipelineLayout; }

    public:
        HPipelineLayout(Context* ctx, Span<PushConstantRange> pushConstantRanges, Span<DescriptorSetLayout> descriptorSetLayouts, BindPoint bindPoint)
            : ctx(ctx)
            , pipelineLayout(ctx->Pipelines_CreateLayout(pushConstantRanges, descriptorSetLayouts, bindPoint))
        {}

        void Destroy()
        {
            ctx->Pipelines_DestroyLayout(pipelineLayout);
        }
    };

// -----------------------------------------------------------------------------
//                               Descriptors
// -----------------------------------------------------------------------------

    struct HDescriptorSet
    {
        Context*               ctx = {};
        DescriptorSet descriptorSet = {};

        operator DescriptorSet() { return descriptorSet; }

    public:
        void Free()
        {
            ctx->Descriptors_FreeSet(descriptorSet);
        }

        void WriteSampledTexture(u32 binding, Texture texture, Sampler sampler, u32 arrayIndex = 0)
        {
            ctx->Descriptors_WriteSampledTexture(descriptorSet, binding, texture, sampler, arrayIndex);
        }

        void WriteUniformBuffer(u32 binding, Buffer buffer, u32 arrayIndex = 0)
        {
            ctx->Descriptors_WriteUniformBuffer(descriptorSet, binding, buffer, arrayIndex);
        }
    };

    struct HDescriptorSetLayout
    {
        Context*                            ctx = {};
        DescriptorSetLayout descriptorSetLayout = {};

        operator DescriptorSetLayout() { return descriptorSetLayout; }

    public:
        HDescriptorSetLayout(Context* ctx, Span<DescriptorBinding> bindings, bool pushDescriptors = false)
            : ctx(ctx)
            , descriptorSetLayout(ctx->Descriptors_CreateSetLayout(bindings, pushDescriptors))
        {}

        void Destroy()
        {
            ctx->Descriptors_DestroySetLayout(descriptorSetLayout);
        }

        HDescriptorSet Allocate(u64 customSize = 0)
        {
            return { ctx, ctx->Descriptors_AllocateSet(descriptorSetLayout, customSize) };
        }
    };

// -----------------------------------------------------------------------------
//                                 Buffer
// -----------------------------------------------------------------------------

    struct HBuffer
    {
        Context*  ctx = {};
        Buffer buffer = {};

        operator Buffer() { return buffer; }

    public:
        HBuffer(Context* ctx, u64 size, BufferUsage usage, BufferFlags flags = {})
            : ctx(ctx)
            , buffer(ctx->Buffer_Create(size, usage, flags))
        {}

        void Destroy()
        {
            ctx->Buffer_Destroy(buffer);
        }

        void Resize(u64 size)
        {
            ctx->Buffer_Resize(buffer, size);
        }

        u64 GetSize()
        {
            return ctx->Buffer_GetSize(buffer);
        }

        b8* GetMapped()
        {
            return ctx->Buffer_GetMapped(buffer);
        }

        u64 GetAddress()
        {
            return ctx->Buffer_GetAddress(buffer);
        }

        template<class T>
        T& Get(u64 index, u64 offset = 0)
        {
            return ctx->Buffer_Get<T>(buffer, offset);
        }
        template<class T>
        void Set(Span<T> elements, u64 index = 0, u64 offset = 0)
        {
            ctx->Buffer_Set<T>(buffer, elements, index, offset);
        }
    };

// -----------------------------------------------------------------------------
//                          Acceleration Structures
// -----------------------------------------------------------------------------

    struct HAccelerationStructureBuilder
    {
        Context*                         ctx = {};
        AccelerationStructureBuilder builder = {};

        operator AccelerationStructureBuilder() { return builder; }

    public:
        HAccelerationStructureBuilder(Context* ctx)
            : ctx(ctx)
            , builder(ctx->AccelerationStructures_CreateBuilder())
        {}

        void Destroy()
        {
            ctx->AccelerationStructures_DestroyBuilder(builder);
        }

        void SetInstances(u32 geometryIndex, u64 deviceAddress, u32 count)
        {
            ctx->AccelerationStructures_SetInstances(builder, geometryIndex, deviceAddress, count);
        }

        void SetTriangles(u32 geometryIndex,
            u64 vertexAddress, Format vertexFormat, u32 vertexStride, u32 maxVertex,
            u64 indexAddress, IndexType indexFormat, u32 triangleCount)
        {
            ctx->AccelerationStructures_SetTriangles(builder, geometryIndex,
                vertexAddress, vertexFormat, vertexStride, maxVertex,
                indexAddress, indexFormat, triangleCount);
        }

        void Prepare(AccelerationStructureType type, AccelerationStructureFlags flags,
            u32 geometryCount, u32 firstGeometry = 0u)
        {
            ctx->AccelerationStructures_Prepare(builder, type, flags, geometryCount, firstGeometry);
        }

        u32 GetInstanceSize()
        {
            return ctx->AccelerationStructures_GetInstanceSize();
        }

        void WriteInstance(
            void* bufferAddress, u32 index,
            AccelerationStructure structure,
            const Mat4& matrix,
            u32 customIndex, u8 mask,
            u32 sbtOffset, GeometryInstanceFlags flags)
        {
            ctx->AccelerationStructures_WriteInstance(bufferAddress, index,
                structure, matrix, customIndex, mask, sbtOffset, flags);
        }

        u64 GetBuildSize()
        {
            return ctx->AccelerationStructures_GetBuildSize(builder);
        }

        u64 GetBuildScratchSize()
        {
            return ctx->AccelerationStructures_GetBuildScratchSize(builder);
        }

        u64 GetUpdateScratchSize()
        {
            return ctx->AccelerationStructures_GetUpdateScratchSize(builder);
        }

        u64 GetCompactSize()
        {
            return ctx->AccelerationStructures_GetCompactSize(builder);
        }
    };

    struct HAccelerationStructure
    {
        Context*                    ctx = {};
        AccelerationStructure structure = {};

        operator AccelerationStructure() { return structure; }

    public:
        HAccelerationStructure(Context* ctx, u64 size, AccelerationStructureType type)
            : ctx(ctx)
            , structure(ctx->AccelerationStructures_Create(size, type))
        {}

        void Destroy()
        {
            ctx->AccelerationStructures_Destroy(structure);
        }

        u64 GetAddress()
        {
            ctx->AccelerationStructures_GetAddress(structure);
        }
    };

// -----------------------------------------------------------------------------
//                              Ray Tracing
// -----------------------------------------------------------------------------

    struct HRayTracingPipeline
    {
        Context*                ctx = {};
        RayTracingPipeline pipeline = {};

        operator RayTracingPipeline() { return pipeline; }

    public:
        HRayTracingPipeline(Context* ctx)
            : ctx(ctx)
            , pipeline(ctx->RayTracing_CreatePipeline())
        {}

        void Destroy()
        {
            ctx->RayTracing_DestroyPipeline(pipeline);
        }

        void Update(PipelineLayout layout,
            Span<Shader> rayGenShaders,
            Span<Shader> rayMissShaders,
            Span<HitShaderGroup> rayHitShaderGroup,
            Span<Shader> callableShaders)
        {
            ctx->RayTracing_UpdatePipeline(pipeline, layout, rayGenShaders, rayMissShaders, rayHitShaderGroup, callableShaders);
        }
    };
}