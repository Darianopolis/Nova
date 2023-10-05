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
        vkDestroyPipeline(impl->context->device, impl->pipeline, impl->context->pAlloc);

        delete impl;
        impl = nullptr;
    }

    void RayTracingPipeline::Update(
        Span<HShader> rayGenShaders,
        Span<HShader> rayMissShaders,
        Span<HitShaderGroup> rayHitShaderGroup,
        Span<HShader> callableShaders) const
    {
        // Convert to stages and groups

        HashMap<VkShaderModule, u32> stageIndices;
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        std::vector<u32> rayGenIndices, rayMissIndices, rayCallIndices;
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

        for (auto& shader : rayGenShaders) {
            rayGenIndices.push_back(u32(groups.size()));
            createGroup().generalShader = getShaderIndex(shader);
        }

        for (auto& shader : rayMissShaders) {
            rayMissIndices.push_back(u32(groups.size()));
            createGroup().generalShader = getShaderIndex(shader);
        }

        impl->rayHitIndices.clear();
        for (auto& group : rayHitShaderGroup) {
            impl->rayHitIndices.push_back(u32(groups.size()));
            auto& info = createGroup();
            info.type = group.intersectionShader
                ? VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR
                : VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            info.closestHitShader = getShaderIndex(group.closestHitShader);
            info.anyHitShader = getShaderIndex(group.anyHitShader);
            info.intersectionShader = getShaderIndex(group.intersectionShader);
        }

        for (auto& shader : callableShaders) {
            rayCallIndices.push_back(u32(groups.size()));
            createGroup().generalShader = getShaderIndex(shader);
        }

        // Create pipeline

        if (impl->pipeline) {
            vkDeviceWaitIdle(impl->context->device);
            vkDestroyPipeline(impl->context->device, impl->pipeline, impl->context->pAlloc);
        }

        vkh::Check(vkCreateRayTracingPipelinesKHR(impl->context->device,
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
        u64 rayMissOffset = AlignUpPower2(rayGenIndices.size() * handleStride, groupAlign);
        u64 rayHitOffset = impl->rayHitOffset = rayMissOffset + AlignUpPower2(rayMissIndices.size() * handleSize, groupAlign);
        u64 rayCallOffset = rayHitOffset + AlignUpPower2(impl->rayHitIndices.size() * handleSize, groupAlign);
        u64 tableSize = rayCallOffset + rayCallIndices.size() * handleStride;

        NOVA_LOGEXPR(handleSize);
        NOVA_LOGEXPR(handleStride);
        NOVA_LOGEXPR(groupAlign);

        // Allocate table and get groups from pipeline

        if (tableSize > impl->sbtBuffer.GetSize()) {
            NOVA_LOG("Resizing existing buffer");
            impl->sbtBuffer.Resize(std::max(256ull, tableSize));
        }

        auto getMapped = [&](u64 offset, u32 i) {
            return impl->sbtBuffer.GetMapped() + offset + (i * handleStride);
        };

        impl->handles.resize(groups.size() * handleSize);
        vkh::Check(vkGetRayTracingShaderGroupHandlesKHR(impl->context->device, impl->pipeline,
            0, u32(groups.size()),
            u32(impl->handles.size()), impl->handles.data()));

        auto getHandle = [&](u32 i) {
            return impl->handles.data() + (i * handleSize);
        };

        // Gen

        impl->rayGenRegion.size = handleSize;
        impl->rayGenRegion.stride = handleStride; // raygen size === stride
        for (u32 i = 0; i < rayGenIndices.size(); ++i) {
            std::memcpy(getMapped(0, i), getHandle(rayGenIndices[i]), handleSize);
        }

        // Miss

        impl->rayMissRegion.deviceAddress = impl->sbtBuffer.GetAddress() + rayMissOffset;
        impl->rayMissRegion.size = rayHitOffset - rayMissOffset;
        impl->rayMissRegion.stride = handleStride;
        for (u32 i = 0; i < rayMissIndices.size(); ++i) {
            std::memcpy(getMapped(rayMissOffset, i), getHandle(rayMissIndices[i]), handleSize);
        }

        // Hit

        impl->rayHitRegion.deviceAddress = impl->sbtBuffer.GetAddress() + rayHitOffset;
        NOVA_LOG("#### Ray Hit Address: {:#x}", impl->rayHitRegion.deviceAddress % groupAlign);
        impl->rayHitRegion.size = rayCallOffset - rayHitOffset;
        impl->rayHitRegion.stride = handleStride;
        for (u32 i = 0; i < impl->rayHitIndices.size(); ++i) {
            std::memcpy(getMapped(rayHitOffset, i), getHandle(impl->rayHitIndices[i]), handleSize);
        }

        // Call

        impl->rayCallRegion.deviceAddress = impl->sbtBuffer.GetAddress() + rayCallOffset;
        impl->rayCallRegion.size = tableSize - rayCallOffset;
        impl->rayCallRegion.stride = handleStride;
        for (u32 i = 0; i < rayCallIndices.size(); ++i) {
            std::memcpy(getMapped(rayCallOffset, i), getHandle(rayCallIndices[i]), handleSize);
        }
    }

    u64 RayTracingPipeline::GetShaderBindingTableSize(u32 numHandles) const
    {
        return std::max(256u, numHandles * impl->handleStride);
    }

    u64 RayTracingPipeline::GetShaderBindingTableAlign() const
    {
        return impl->context->rayTracingPipelineProperties.shaderGroupBaseAlignment;
    }

    void RayTracingPipeline::WriteHandle(void* bufferAddress, u32 index, u32 groupIndex)
    {
        std::memcpy(
            static_cast<b8*>(bufferAddress) + index * impl->handleStride,
            impl->handles.data() + impl->rayHitIndices[groupIndex] * impl->handleStride,
            impl->handleSize);
    }

    void CommandList::TraceRays(HRayTracingPipeline pipeline, Vec3U extent, u32 genIndex, u64 hitShaderAddress, u32 hitShaderCount) const
    {
        vkCmdBindPipeline(impl->buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->pipeline);

        auto rayGenRegion = pipeline->rayGenRegion;
        rayGenRegion.deviceAddress = pipeline->sbtBuffer.GetAddress() + (rayGenRegion.stride * genIndex);

        VkStridedDeviceAddressRegionKHR rayHitRegion = {};
        if (hitShaderAddress) {
            rayHitRegion.deviceAddress = hitShaderAddress;
            rayHitRegion.size = hitShaderCount * pipeline->rayHitRegion.stride;
            rayHitRegion.stride = pipeline->rayHitRegion.stride;
        } else {
            rayHitRegion = pipeline->rayHitRegion;
        }

        vkCmdTraceRaysKHR(impl->buffer,
            &rayGenRegion,
            &pipeline->rayMissRegion,
            &rayHitRegion,
            &pipeline->rayCallRegion,
            extent.x, extent.y, extent.z);
    }
}