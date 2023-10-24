#include "nova_VulkanRHI.hpp"

namespace nova
{
    RayTracingPipeline RayTracingPipeline::Create(HContext context)
    {
        auto impl = new Impl;
        impl->context = context;

        impl->sbtBuffer = nova::Buffer::Create(context, 0,
            BufferUsage::ShaderBindingTable,
            BufferFlags::DeviceLocal | BufferFlags::Mapped);

        impl->handleSize = impl->context->rayTracingPipelineProperties.shaderGroupHandleSize;
        impl->handleStride = u32(AlignUpPower2(impl->handleSize,
            impl->context->rayTracingPipelineProperties.shaderGroupHandleAlignment));

        return { impl };
    }

    void RayTracingPipeline::Destroy()
    {
        if (!impl) {
            return;
        }

        impl->sbtBuffer.Destroy();
        impl->context->vkDestroyPipeline(impl->context->device, impl->pipeline, impl->context->pAlloc);

        delete impl;
        impl = nullptr;
    }

    void RayTracingPipeline::Update(
        HShader                   rayGenShader,
        Span<HShader>           rayMissShaders,
        Span<HitShaderGroup> rayHitShaderGroup,
        Span<HShader>          callableShaders) const
    {
        // Convert to stages and groups

        HashMap<VkShaderModule, u32> stageIndices;
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        u32 rayGenIndex;
        u32 rayMissIndexStart;
        u32 rayCallIndexStart;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

        auto getShaderIndex = [&](Shader shader) {
            if (!shader) {
                return VK_SHADER_UNUSED_KHR;
            }

            if (!stageIndices.contains(shader->handle)) {
                stageIndices.insert({ shader->handle, u32(stages.size()) });
                stages.push_back(shader->GetStageInfo());
            }

            return stageIndices.at(shader->handle);
        };

        auto createGroup = [&]() -> auto& {
            auto& info = groups.emplace_back();
            info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            info.generalShader = info.closestHitShader = info.anyHitShader = info.intersectionShader = VK_SHADER_UNUSED_KHR;
            return info;
        };

        // Ray Hit groups
        for (auto& group : rayHitShaderGroup) {
            auto& info = createGroup();
            info.type = group.intersectionShader
                ? VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR
                : VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            info.closestHitShader = getShaderIndex(group.closestHitShader);
            info.anyHitShader = getShaderIndex(group.anyHitShader);
            info.intersectionShader = getShaderIndex(group.intersectionShader);
        }

        // Ray Gen group
        rayGenIndex = u32(groups.size());
        createGroup().generalShader = getShaderIndex(rayGenShader);

        // Ray Miss groups
        rayMissIndexStart = u32(groups.size());
        for (auto& shader : rayMissShaders) {
            createGroup().generalShader = getShaderIndex(shader);
        }

        // Ray Call groups
        rayCallIndexStart = u32(groups.size());
        for (auto& shader : callableShaders) {
            createGroup().generalShader = getShaderIndex(shader);
        }

        // Create pipeline

        if (impl->pipeline) {
            impl->context->vkDestroyPipeline(impl->context->device, impl->pipeline, impl->context->pAlloc);
        }

        vkh::Check(impl->context->vkCreateRayTracingPipelinesKHR(impl->context->device,
            0, impl->context->pipelineCache,
            1, Temp(VkRayTracingPipelineCreateInfoKHR {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
                .flags = impl->context->descriptorBuffers
                    ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                    : VkPipelineCreateFlags(0),
                .stageCount = u32(stages.size()),
                .pStages = stages.data(),
                .groupCount = u32(groups.size()),
                .pGroups = groups.data(),
                .maxPipelineRayRecursionDepth = 2, // TODO: Parameterize
                .layout = impl->context->pipelineLayout,
            }),
            impl->context->pAlloc, &impl->pipeline));

        // Compute table parameters

        u32 handleSize = impl->handleSize;
        u32 handleStride = impl->handleStride;
        u64 groupAlign = impl->context->rayTracingPipelineProperties.shaderGroupBaseAlignment;
        u64 rayGenOffset  = AlignUpPower2(                rayHitShaderGroup.size() * handleStride, groupAlign);
        u64 rayMissOffset = AlignUpPower2(rayGenOffset  +                            handleStride, groupAlign);
        u64 rayCallOffset = AlignUpPower2(rayMissOffset +    rayMissShaders.size() * handleStride, groupAlign);
        u64 tableSize     =               rayCallOffset +   callableShaders.size() * handleStride;

        // Allocate table and get groups from pipeline

        if (tableSize > impl->sbtBuffer.GetSize()) {
            NOVA_LOG("Resizing existing buffer");
            impl->sbtBuffer.Resize(std::max(256ull, tableSize));
        }

        auto getMapped = [&](u64 offset, u32 i) {
            return impl->sbtBuffer.GetMapped() + offset + (i * handleStride);
        };

        impl->handles.resize(groups.size() * handleSize);
        vkh::Check(impl->context->vkGetRayTracingShaderGroupHandlesKHR(impl->context->device, impl->pipeline,
            0, u32(groups.size()),
            u32(impl->handles.size()), impl->handles.data()));

        auto getHandle = [&](u32 i) {
            return impl->handles.data() + (i * handleSize);
        };

        // Hit

        impl->rayHitRegion.deviceAddress = impl->sbtBuffer.GetAddress();
        impl->rayHitRegion.size = rayGenOffset;
        impl->rayHitRegion.stride = handleStride;
        for (u32 i = 0; i < rayHitShaderGroup.size(); ++i) {
            std::memcpy(getMapped(0, i), getHandle(i), handleSize);
        }

        // Gen

        impl->rayGenRegion.deviceAddress = impl->sbtBuffer.GetAddress() + rayGenOffset;
        impl->rayGenRegion.size = handleStride;
        impl->rayGenRegion.stride = handleStride; // raygen size === stride
        std::memcpy(getMapped(rayGenOffset, 0), getHandle(rayGenIndex), handleSize);

        // Miss

        impl->rayMissRegion.deviceAddress = impl->sbtBuffer.GetAddress() + rayMissOffset;
        impl->rayMissRegion.size = rayCallOffset - rayMissOffset;
        impl->rayMissRegion.stride = handleStride;
        for (u32 i = 0; i < rayMissShaders.size(); ++i) {
            std::memcpy(getMapped(rayMissOffset, i), getHandle(rayMissIndexStart + i), handleSize);
        }

        // Call

        impl->rayCallRegion.deviceAddress = impl->sbtBuffer.GetAddress() + rayCallOffset;
        impl->rayCallRegion.size = tableSize - rayCallOffset;
        impl->rayCallRegion.stride = handleStride;
        for (u32 i = 0; i < callableShaders.size(); ++i) {
            std::memcpy(getMapped(rayCallOffset, i), getHandle(rayCallIndexStart + i), handleSize);
        }
    }

    u64 RayTracingPipeline::GetTableSize(u32 handles) const
    {
        return std::max(256ull, u64(handles) * impl->handleStride);
    }

    u64 RayTracingPipeline::GetHandleSize() const
    {
        return impl->handleSize;
    }

    u64 RayTracingPipeline::GetHandleGroupAlign() const
    {
        return impl->context->rayTracingPipelineProperties.shaderGroupBaseAlignment;
    }

    u64 RayTracingPipeline::GetHandleStride() const
    {
        return impl->handleStride;
    }

    HBuffer RayTracingPipeline::GetHandles() const
    {
        return impl->sbtBuffer;
    }

    void RayTracingPipeline::WriteHandle(void* bufferAddress, u32 index, u32 groupIndex)
    {
        std::memcpy(
            static_cast<b8*>(bufferAddress) + index * impl->handleStride,
            impl->handles.data() + (groupIndex * impl->handleStride),
            impl->handleSize);
    }

    void CommandList::TraceRays(HRayTracingPipeline pipeline, Vec3U extent, u64 hitShaderAddress, u32 hitShaderCount) const
    {
        impl->context->vkCmdBindPipeline(impl->buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->pipeline);

        VkStridedDeviceAddressRegionKHR rayHitRegion = {};
        if (hitShaderAddress) {
            rayHitRegion.deviceAddress = hitShaderAddress;
            rayHitRegion.size = hitShaderCount * pipeline->rayHitRegion.stride;
            rayHitRegion.stride = pipeline->rayHitRegion.stride;
        } else {
            rayHitRegion = pipeline->rayHitRegion;
        }

        impl->context->vkCmdTraceRaysKHR(impl->buffer,
            &pipeline->rayGenRegion,
            &pipeline->rayMissRegion,
            &rayHitRegion,
            &pipeline->rayCallRegion,
            extent.x, extent.y, extent.z);
    }
}