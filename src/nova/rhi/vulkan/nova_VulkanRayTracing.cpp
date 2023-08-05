#include "nova_VulkanRHI.hpp"

namespace nova
{
    RayTracingPipeline::RayTracingPipeline(HContext _context)
        : Object(_context)
    {
        sbtBuffer = std::make_unique<Buffer>(context, 0,
            BufferUsage::ShaderBindingTable,
            BufferFlags::DeviceLocal | BufferFlags::Mapped);
    }

    RayTracingPipeline::~RayTracingPipeline()
    {
        vkDestroyPipeline(context->device, pipeline, context->pAlloc);
    }

    void RayTracingPipeline::Update(HPipelineLayout layout,
        Span<HShader> rayGenShaders,
        Span<HShader> rayMissShaders,
        Span<HitShaderGroup> rayHitShaderGroup,
        Span<HShader> callableShaders)
    {
        // Convert to stages and groups

        ankerl::unordered_dense::map<VkShaderModule, u32> stageIndices;
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        std::vector<u32> rayGenIndices, rayMissIndices, rayHitIndices, rayCallIndices;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

        auto getShaderIndex = [&](HShader shader) {
            if (!shader)
                return VK_SHADER_UNUSED_KHR;

            if (!stageIndices.contains(shader->handle))
            {
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

        for (auto& shader : rayGenShaders)
        {
            rayGenIndices.push_back(u32(groups.size()));
            createGroup().generalShader = getShaderIndex(shader);
        }

        for (auto& shader : rayMissShaders)
        {
            rayMissIndices.push_back(u32(groups.size()));
            createGroup().generalShader = getShaderIndex(shader);
        }

        for (auto& group : rayHitShaderGroup)
        {
            rayHitIndices.push_back(u32(groups.size()));
            auto& info = createGroup();
            info.type = group.intersectionShader
                ? VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR
                : VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            info.closestHitShader = getShaderIndex(group.closestHitShader);
            info.anyHitShader = getShaderIndex(group.anyHitShader);
            info.intersectionShader = getShaderIndex(group.intersectionShader);
        }

        for (auto& shader : callableShaders)
        {
            rayCallIndices.push_back(u32(groups.size()));
            createGroup().generalShader = getShaderIndex(shader);
        }

        // Create pipeline

        if (pipeline)
            vkDestroyPipeline(context->device, pipeline, context->pAlloc);

        VkCall(vkCreateRayTracingPipelinesKHR(context->device,
            0, context->pipelineCache,
            1, Temp(VkRayTracingPipelineCreateInfoKHR {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
                .flags = context->config.descriptorBuffers
                    ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                    : VkPipelineCreateFlags(0),
                .stageCount = u32(stages.size()),
                .pStages = stages.data(),
                .groupCount = u32(groups.size()),
                .pGroups = groups.data(),
                .maxPipelineRayRecursionDepth = 2, // TODO: Parameterize
                .layout = layout->layout,
            }),
            context->pAlloc, &pipeline));

        // Compute table parameters

        u32 handleSize = context->rayTracingPipelineProperties.shaderGroupHandleSize;
        u32 handleStride = u32(AlignUpPower2(handleSize, context->rayTracingPipelineProperties.shaderGroupHandleAlignment));
        u64 groupAlign = context->rayTracingPipelineProperties.shaderGroupBaseAlignment;
        u64 rayMissOffset = AlignUpPower2(rayGenIndices.size() * handleStride, groupAlign);
        u64 rayHitOffset = rayMissOffset + AlignUpPower2(rayMissIndices.size() * handleSize, groupAlign);
        u64 rayCallOffset = rayHitOffset + AlignUpPower2(rayHitIndices.size() * handleSize, groupAlign);
        u64 tableSize = rayCallOffset + rayCallIndices.size() * handleStride;

        // Allocate table and get groups from pipeline

        if (tableSize > sbtBuffer->GetSize())
        {
            NOVA_LOG("Resizing existing buffer");
            sbtBuffer->Resize(std::max(256ull, tableSize));
        }

        auto getMapped = [&](u64 offset, u32 i) { return sbtBuffer->GetMapped() + offset + (i * handleStride); };

        std::vector<u8> handles(groups.size() * handleSize);
        VkCall(vkGetRayTracingShaderGroupHandlesKHR(context->device, pipeline,
            0, u32(groups.size()),
            u32(handles.size()), handles.data()));

        auto getHandle = [&](u32 i) { return handles.data() + (i * handleSize); };

        // Gen

        rayGenRegion.size = handleSize;
        rayGenRegion.stride = handleStride; // raygen size === stride
        for (u32 i = 0; i < rayGenIndices.size(); ++i)
            std::memcpy(getMapped(0, i), getHandle(rayGenIndices[i]), handleSize);

        // Miss

        rayMissRegion.deviceAddress = sbtBuffer->GetAddress() + rayMissOffset;
        rayMissRegion.size = rayHitOffset - rayMissOffset;
        rayMissRegion.stride = handleStride;
        for (u32 i = 0; i < rayMissIndices.size(); ++i)
            std::memcpy(getMapped(rayMissOffset, i), getHandle(rayMissIndices[i]), handleSize);

        // Hit

        rayHitRegion.deviceAddress = sbtBuffer->GetAddress() + rayHitOffset;
        rayHitRegion.size = rayCallOffset - rayHitOffset;
        rayHitRegion.stride = handleStride;
        for (u32 i = 0; i < rayHitIndices.size(); ++i)
            std::memcpy(getMapped(rayHitOffset, i), getHandle(rayHitIndices[i]), handleSize);

        // Call

        rayCallRegion.deviceAddress = sbtBuffer->GetAddress() + rayCallOffset;
        rayCallRegion.size = tableSize - rayCallOffset;
        rayCallRegion.stride = handleStride;
        for (u32 i = 0; i < rayCallIndices.size(); ++i)
            std::memcpy(getMapped(rayCallOffset, i), getHandle(rayCallIndices[i]), handleSize);
    }

    void CommandList::TraceRays(HRayTracingPipeline pipeline, Vec3U extent, u32 genIndex)
    {
        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->pipeline);

        auto rayGenRegion = pipeline->rayGenRegion;
        rayGenRegion.deviceAddress = pipeline->sbtBuffer->GetAddress() + (rayGenRegion.stride * genIndex);

        vkCmdTraceRaysKHR(buffer,
            &rayGenRegion,
            &pipeline->rayMissRegion,
            &pipeline->rayHitRegion,
            &pipeline->rayCallRegion,
            extent.x, extent.y, extent.z);
    }
}