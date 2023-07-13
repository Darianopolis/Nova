#pragma once

#include "nova_RHI.hpp"

namespace nova
{
    struct HQueue
    {
        Context* ctx = {};
        Queue  queue = {};

        operator Queue() const { return queue; }

    public:
        bool IsValid() const { return ctx && ctx->IsValid(queue); }

        void Submit(Span<CommandList> commandLists, Span<Fence> waits, Span<Fence> signals) const
        {
            ctx->Queue_Submit(queue, commandLists, waits, signals);
        }

        bool Acquire(Span<Swapchain> swapchains, Span<Fence> signals) const
        {
            return ctx->Queue_Acquire(queue, swapchains, signals);
        }

        void Present(Span<Swapchain> swapchains, Span<Fence> waits, bool hostWait = false) const
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

        operator Fence() const { return fence; };

    public:
        bool IsValid() const { return ctx && ctx->IsValid(fence); }

        void Destroy() const
        {
            ctx->Fence_Destroy(fence);
        }

        void Wait(u64 waitValue = 0ull) const
        {
            ctx->Fence_Wait(fence, waitValue);
        }

        u64 Advance() const
        {
            return ctx->Fence_Advance(fence);
        }

        void Signal(u64 signalValue = 0ull) const
        {
            ctx->Fence_Signal(fence, signalValue);
        }

        u64 GetPendingValue() const
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

        operator CommandState() const { return state; }

    public:
        void SetState(Texture texture,
            VkImageLayout layout, VkPipelineStageFlags2 stages, VkAccessFlags2 access) const
        {
            ctx->Commands_SetState(state, texture, layout, stages, access);
        }

        void Destroy() const
        {
            ctx->Commands_DestroyState(state);
        }
    };

    struct HCommandList
    {
        Context*    ctx = {};
        CommandList cmd = {};

        operator CommandList() const { return cmd; }

    public:
        bool IsValid() const { return ctx && ctx->IsValid(cmd); }

        // Textures

        void Transition(Texture texture, TextureLayout layout, PipelineStage stage) const
        {
            ctx->Cmd_Transition(cmd, texture, layout, stage);
        }

        void Clear(Texture texture, Vec4 color) const
        {
            ctx->Cmd_Clear(cmd, texture, color);
        }

        void CopyToTexture(Texture dst, Buffer src, u64 srcOffset = 0) const
        {
            ctx->Cmd_CopyToTexture(cmd, dst, src, srcOffset);
        }

        void CopyFromTexture(Buffer dst, Texture src, Rect2D region) const
        {
            ctx->Cmd_CopyFromTexture(cmd, dst, src, region);
        }

        void GenerateMips(Texture texture) const
        {
            ctx->Cmd_GenerateMips(cmd, texture);
        }

        void BlitImage(Texture dst, Texture src, Filter filter) const
        {
            ctx->Cmd_BlitImage(cmd, dst, src, filter);
        }

        // Swapchain

        void Present(Swapchain swapchain) const
        {
            ctx->Cmd_Present(cmd, swapchain);
        }

        // Pipelines

        void SetGraphicsState(PipelineLayout layout, Span<Shader> shaders, const PipelineState& state) const
        {
            ctx->Cmd_SetGraphicsState(cmd, layout, shaders, state);
        }

        void PushConstants(PipelineLayout layout, u64 offset, u64 size, const void* data) const
        {
            ctx->Cmd_PushConstants(cmd, layout, offset, size, data);
        }

        // Commands

        void Barrier(PipelineStage src, PipelineStage dst) const
        {
            ctx->Cmd_Barrier(cmd, src, dst);
        }

        // Drawing

        void BeginRendering(Rect2D region, Span<Texture> colorAttachments, Texture depthAttachment = {}, Texture stencilAttachment = {}) const
        {
            ctx->Cmd_BeginRendering(cmd, region, colorAttachments, depthAttachment, stencilAttachment);
        }

        void EndRendering() const
        {
            ctx->Cmd_EndRendering(cmd);
        }

        void Draw(u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance) const
        {
            ctx->Cmd_Draw(cmd, vertices, instances, firstVertex, firstInstance);
        }

        void DrawIndexed(u32 indices, u32 instances, u32 firstIndex, u32 vertexOffset, u32 firstInstance) const
        {
            ctx->Cmd_DrawIndexed(cmd, indices, instances, firstIndex, vertexOffset, firstInstance);
        }

        void BindIndexBuffer(Buffer buffer, IndexType indexType, u64 offset = 0) const
        {
            ctx->Cmd_BindIndexBuffer(cmd, buffer, indexType, offset);
        }

        void ClearColor(u32 attachment, Vec4 color, Vec2U size, Vec2I offset = {}) const
        {
            ctx->Cmd_ClearColor(cmd, attachment, color, size, offset);
        }

        void ClearDepth(f32 depth, Vec2U size, Vec2I offset = {}) const
        {
            ctx->Cmd_ClearDepth(cmd, depth, size, offset);
        }

        void ClearStencil(u32 value, Vec2U size, Vec2I offset = {}) const
        {
            ctx->Cmd_ClearStencil(cmd, value, size, offset);
        }

        // Descriptors

        void BindDescriptorSets(PipelineLayout pipelineLayout, u32 firstSet, Span<DescriptorSet> sets) const
        {
            ctx->Cmd_BindDescriptorSets(cmd, pipelineLayout, firstSet, sets);
        }

        void PushStorageTexture(PipelineLayout layout, u32 setIndex, u32 binding, Texture texture, u32 arrayIndex = 0) const
        {
            ctx->Cmd_PushStorageTexture(cmd, layout, setIndex, binding, texture, arrayIndex);
        }

        void PushAccelerationStructure(PipelineLayout layout, u32 setIndex, u32 binding, AccelerationStructure accelerationStructure, u32 arrayIndex = 0) const
        {
            ctx->Cmd_PushAccelerationStructure(cmd, layout, setIndex, binding, accelerationStructure, arrayIndex);
        }

        // Buffers

        void UpdateBuffer(Buffer dst, const void* pData, usz size, u64 dstOffset = 0) const
        {
            ctx->Cmd_UpdateBuffer(cmd, dst, pData, size, dstOffset);
        }

        void CopyToBuffer(Buffer dst, Buffer src, u64 size, u64 dstOffset = 0, u64 srcOffset = 0) const
        {
            ctx->Cmd_CopyToBuffer(cmd, dst, src, size, dstOffset, srcOffset);
        }

        void Barrier(Buffer buffer, PipelineStage src, PipelineStage dst) const
        {
            ctx->Cmd_Barrier(cmd, buffer, src, dst);
        }

        // Acceleration Structures

        void BuildAccelerationStructure(AccelerationStructureBuilder builder, AccelerationStructure structure, Buffer scratch) const
        {
            ctx->Cmd_BuildAccelerationStructure(cmd, builder, structure, scratch);
        }

        void CompactAccelerationStructure(AccelerationStructure dst, AccelerationStructure src) const
        {
            ctx->Cmd_CompactAccelerationStructure(cmd, dst, src);
        }

        // Ray Tracing

        void TraceRays(RayTracingPipeline pipeline, Vec3U extent, u32 genIndex) const
        {
            ctx->Cmd_TraceRays(cmd, pipeline, extent, genIndex);
        }

        // Compute

        void SetComputeState(PipelineLayout layout, Shader shader) const
        {
            ctx->Cmd_SetComputeState(cmd, layout, shader);
        }

        void Dispatch(Vec3U groups) const
        {
            ctx->Cmd_Dispatch(cmd, groups);
        }
    };

    struct HCommandPool
    {
        Context*     ctx = {};
        CommandPool pool = {};

        operator CommandPool() const { return pool; }

    public:
        bool IsValid() const { return ctx && ctx->IsValid(pool); }

        void Destroy() const
        {
            ctx->Commands_DestroyPool(pool);
        }

        HCommandList Begin(CommandState state) const
        {
            return { ctx, ctx->Commands_Begin(pool, state) };
        }

        void Reset() const
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

        operator Sampler() const { return sampler; }

    public:
        bool IsValid() const { return ctx && ctx->IsValid(sampler); }

        void Destroy() const
        {
            ctx->Sampler_Destroy(sampler);
        }
    };

    struct HTexture
    {
        Context*    ctx = {};
        Texture texture = {};

        operator Texture() const { return texture; }

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

        bool IsValid() const { return ctx && ctx->IsValid(texture); }

        void Destroy() const
        {
            ctx->Texture_Destroy(texture);
        }

        Vec3U GetExtent() const
        {
            return ctx->Texture_GetExtent(texture);
        }

        Format GetFormat() const
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

        operator Swapchain() const { return swapchain; }

    public:
        bool IsValid() const { return ctx && ctx->IsValid(swapchain); }

        void Destroy() const
        {
            ctx->Swapchain_Destroy(swapchain);
        }

        HTexture GetCurrent() const
        {
            return { ctx, ctx->Swapchain_GetCurrent(swapchain) };
        }

        Vec2U GetExtent() const
        {
            return ctx->Swapchain_GetExtent(swapchain);
        }

        Format GetFormat() const
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

        operator Shader() const { return shader; }

    public:
        bool IsValid() const { return ctx && ctx->IsValid(shader); }

        void Destroy() const
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

        operator PipelineLayout() const { return pipelineLayout; }

    public:
        bool IsValid() const { return ctx && ctx->IsValid(pipelineLayout); }

        void Destroy() const
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

        operator DescriptorSet() const { return descriptorSet; }

    public:
        bool IsValid() const { return ctx && ctx->IsValid(descriptorSet); }

        void Free() const
        {
            ctx->Descriptors_FreeSet(descriptorSet);
        }

        void WriteSampledTexture(u32 binding, Texture texture, Sampler sampler, u32 arrayIndex = 0) const
        {
            ctx->Descriptors_WriteSampledTexture(descriptorSet, binding, texture, sampler, arrayIndex);
        }

        void WriteUniformBuffer(u32 binding, Buffer buffer, u32 arrayIndex = 0) const
        {
            ctx->Descriptors_WriteUniformBuffer(descriptorSet, binding, buffer, arrayIndex);
        }
    };

    struct HDescriptorSetLayout
    {
        Context*                            ctx = {};
        DescriptorSetLayout descriptorSetLayout = {};

        operator DescriptorSetLayout() const { return descriptorSetLayout; }

    public:
        bool IsValid() const { return ctx && ctx->IsValid(descriptorSetLayout); }

        void Destroy() const
        {
            ctx->Descriptors_DestroySetLayout(descriptorSetLayout);
        }

        HDescriptorSet Allocate(u64 customSize = 0) const
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

        operator Buffer() const { return buffer; }

    public:
        bool IsValid() const { return ctx && ctx->IsValid(buffer); }

        void Destroy() const
        {
            ctx->Buffer_Destroy(buffer);
        }

        void Resize(u64 size) const
        {
            ctx->Buffer_Resize(buffer, size);
        }

        u64 GetSize() const
        {
            return ctx->Buffer_GetSize(buffer);
        }

        b8* GetMapped() const
        {
            return ctx->Buffer_GetMapped(buffer);
        }

        u64 GetAddress() const
        {
            return ctx->Buffer_GetAddress(buffer);
        }

        template<typename T>
        T& Get(u64 index, u64 offset = 0) const
        {
            return ctx->Buffer_Get<T>(buffer, index, offset);
        }
        template<typename T>
        void Set(Span<T> elements, u64 index = 0, u64 offset = 0) const
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

        operator AccelerationStructureBuilder() const { return builder; }

    public:
        bool IsValid() const { return ctx && ctx->IsValid(builder); }

        void Destroy() const
        {
            ctx->AccelerationStructures_DestroyBuilder(builder);
        }

        void SetInstances(u32 geometryIndex, u64 deviceAddress, u32 count) const
        {
            ctx->AccelerationStructures_SetInstances(builder, geometryIndex, deviceAddress, count);
        }

        void SetTriangles(u32 geometryIndex,
            u64 vertexAddress, Format vertexFormat, u32 vertexStride, u32 maxVertex,
            u64 indexAddress, IndexType indexFormat, u32 triangleCount) const
        {
            ctx->AccelerationStructures_SetTriangles(builder, geometryIndex,
                vertexAddress, vertexFormat, vertexStride, maxVertex,
                indexAddress, indexFormat, triangleCount);
        }

        void Prepare(AccelerationStructureType type, AccelerationStructureFlags flags,
            u32 geometryCount, u32 firstGeometry = 0u) const
        {
            ctx->AccelerationStructures_Prepare(builder, type, flags, geometryCount, firstGeometry);
        }

        u32 GetInstanceSize() const
        {
            return ctx->AccelerationStructures_GetInstanceSize();
        }

        void WriteInstance(
            void* bufferAddress, u32 index,
            AccelerationStructure structure,
            const Mat4& matrix,
            u32 customIndex, u8 mask,
            u32 sbtOffset, GeometryInstanceFlags flags) const
        {
            ctx->AccelerationStructures_WriteInstance(bufferAddress, index,
                structure, matrix, customIndex, mask, sbtOffset, flags);
        }

        u64 GetBuildSize() const
        {
            return ctx->AccelerationStructures_GetBuildSize(builder);
        }

        u64 GetBuildScratchSize() const
        {
            return ctx->AccelerationStructures_GetBuildScratchSize(builder);
        }

        u64 GetUpdateScratchSize() const
        {
            return ctx->AccelerationStructures_GetUpdateScratchSize(builder);
        }

        u64 GetCompactSize() const
        {
            return ctx->AccelerationStructures_GetCompactSize(builder);
        }
    };

    struct HAccelerationStructure
    {
        Context*                    ctx = {};
        AccelerationStructure structure = {};

        operator AccelerationStructure() const { return structure; }

    public:
        bool IsValid() const { return ctx && ctx->IsValid(structure); }

        void Destroy() const
        {
            ctx->AccelerationStructures_Destroy(structure);
        }

        u64 GetAddress() const
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

        operator RayTracingPipeline() const { return pipeline; }

    public:
        bool IsValid() const { return ctx && ctx->IsValid(pipeline); }

        void Destroy() const
        {
            ctx->RayTracing_DestroyPipeline(pipeline);
        }

        void Update(PipelineLayout layout,
            Span<Shader> rayGenShaders,
            Span<Shader> rayMissShaders,
            Span<HitShaderGroup> rayHitShaderGroup,
            Span<Shader> callableShaders) const
        {
            ctx->RayTracing_UpdatePipeline(pipeline, layout, rayGenShaders, rayMissShaders, rayHitShaderGroup, callableShaders);
        }
    };

    struct HContext
    {
        Context* ctx;

        operator Context*() const { return ctx; }

    public:
        const ContextConfig GetConfig() const
        {
            return ctx->GetConfig();
        }

        void WaitIdle() const
        {
            return ctx->WaitIdle();
        }

    public:
        HQueue GetQueue(QueueFlags flags, u32 index) const
        {
            return { ctx, ctx->Queue_Get(flags, index) };
        }

        HFence CreateFence() const
        {
            return { ctx, ctx->Fence_Create() };
        }

        HCommandState CreateCommandState() const
        {
            return { ctx, ctx->Commands_CreateState() };
        }

        HCommandPool CreateCommandPool(Queue queue) const
        {
            return { ctx, ctx->Commands_CreatePool(queue) };
        }

        HSampler CreateSampler(Filter filter, AddressMode addressMode, BorderColor color, f32 anisotropy = 0.f) const
        {
            return { ctx, ctx->Sampler_Create(filter, addressMode, color, anisotropy) };
        };

        HTexture CreateTexture(Vec3U size, TextureUsage usage, Format format, TextureFlags flags = {}) const
        {
            return { ctx, ctx->Texture_Create(size, usage, format, flags) };
        }

        HSwapchain CreateSwapchain(void* window, TextureUsage usage, PresentMode presentMode) const
        {
            return { ctx, ctx->Swapchain_Create(window, usage, presentMode) };
        }

        HShader CreateShader(ShaderStage stage, const std::string& filename, const std::string& sourceCode) const
        {
            return { ctx, ctx->Shader_Create(stage, filename, sourceCode) };
        }

        HShader CreateShader(ShaderStage stage, Span<ShaderElement> elements) const
        {
            return { ctx, ctx->Shader_Create(stage, elements) };
        }

        HPipelineLayout CreatePipelineLayout(Span<PushConstantRange> pushConstantRanges, Span<DescriptorSetLayout> descriptorSetLayouts, BindPoint bindPoint) const
        {
            return { ctx, ctx->Pipelines_CreateLayout(pushConstantRanges, descriptorSetLayouts, bindPoint) };
        }

        HDescriptorSetLayout CreateDescriptorSetLayout(Span<DescriptorBinding> bindings, bool pushDescriptors = false) const
        {
            return { ctx, ctx->Descriptors_CreateSetLayout(bindings, pushDescriptors) };
        }

        HBuffer CreateBuffer(u64 size, BufferUsage usage, BufferFlags flags = {}) const
        {
            return { ctx, ctx->Buffer_Create(size, usage, flags) };
        }

        HAccelerationStructureBuilder CreateAccelerationStructureBuilder() const
        {
            return { ctx, ctx->AccelerationStructures_CreateBuilder() };
        }

        HAccelerationStructure CreateAccelerationStructure(u64 size, AccelerationStructureType type, Buffer buffer = {}, u64 offset = {}) const
        {
            return { ctx, ctx->AccelerationStructures_Create(size, type, buffer, offset) };
        }

        HRayTracingPipeline CreateRayTracingPipeline() const
        {
            return { ctx, ctx->RayTracing_CreatePipeline() };
        }
    };
}