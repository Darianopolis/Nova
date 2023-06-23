#include "nova_VulkanRHI.hpp"

namespace nova
{
    RayTracingPipeline VulkanContext::RayTracing_CreatePipeline()
    {
        auto[id, pipeline] = rayTracingPipelines.Acquire();
        pipeline.sbtBuffer = {};
        return id;
    }

    void VulkanContext::RayTracing_DestroyPipeline(RayTracingPipeline pipeline)
    {
        vkDestroyPipeline(device, Get(pipeline).pipeline, pAlloc);
        if (buffers.IsValid(Get(pipeline).sbtBuffer))
            Buffer_Destroy(Get(pipeline).sbtBuffer);
        rayTracingPipelines.Return(pipeline);
    }

    void VulkanContext::RayTracing_UpdatePipeline(RayTracingPipeline pipeline,
        PipelineLayout layout,
        Span<Shader> rayGenShaders,
        Span<Shader> rayMissShaders,
        Span<HitShaderGroup> rayHitShaderGroup,
        Span<Shader> callableShaders)
    {
        // Convert to stages and groups

        ankerl::unordered_dense::map<VkShaderModule, u32> stageIndices;
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        std::vector<u32> rayGenIndices, rayMissIndices, rayHitIndices, rayCallIndices;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

        constexpr auto getStageInfo = [](VulkanShader& shader) {
            return VkPipelineShaderStageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = shader.stage,
                .module = shader.handle,
                .pName = VulkanShader::EntryPoint,
            };
        };

        auto getShaderIndex = [&](Shader shader) {
            if (!shaders.IsValid(shader))
                return VK_SHADER_UNUSED_KHR;

            if (!stageIndices.contains(Get(shader).handle))
            {
                stageIndices.insert({ Get(shader).handle, u32(stages.size()) });
                stages.push_back(getStageInfo(Get(shader)));
            }

            return stageIndices.at(Get(shader).handle);
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
            info.type = shaders.IsValid(group.intersectionShader)
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

        if (Get(pipeline).pipeline)
            vkDestroyPipeline(device, Get(pipeline).pipeline, pAlloc);

        VkCall(vkCreateRayTracingPipelinesKHR(device,
            0, pipelineCache,
            1, Temp(VkRayTracingPipelineCreateInfoKHR {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
                .flags = config.descriptorBuffers
                    ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                    : VkPipelineCreateFlags(0),
                .stageCount = u32(stages.size()),
                .pStages = stages.data(),
                .groupCount = u32(groups.size()),
                .pGroups = groups.data(),
                .maxPipelineRayRecursionDepth = 2, // TODO: Parameterize
                .layout = Get(layout).layout,
            }),
            pAlloc, &Get(pipeline).pipeline));

        // Compute table parameters

        u32 handleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
        u32 handleStride = u32(AlignUpPower2(handleSize, rayTracingPipelineProperties.shaderGroupHandleAlignment));
        u64 groupAlign = rayTracingPipelineProperties.shaderGroupBaseAlignment;
        u64 rayMissOffset = AlignUpPower2(rayGenIndices.size() * handleStride, groupAlign);
        u64 rayHitOffset = rayMissOffset + AlignUpPower2(rayMissIndices.size() * handleSize, groupAlign);
        u64 rayCallOffset = rayHitOffset + AlignUpPower2(rayHitIndices.size() * handleSize, groupAlign);
        u64 tableSize = rayCallOffset + rayCallIndices.size() * handleStride;

        // Allocate table and get groups from pipeline

        if (!buffers.IsValid(Get(pipeline).sbtBuffer))
        {
            NOVA_LOG("Generating new buffer");
            Get(pipeline).sbtBuffer = Buffer_Create(
                std::max(256ull, tableSize),
                BufferUsage::ShaderBindingTable,
                BufferFlags::DeviceLocal | BufferFlags::Mapped);
        }
        else
        if (tableSize > Buffer_GetSize(Get(pipeline).sbtBuffer))
        {
            NOVA_LOG("Resizing existing buffer");
            Buffer_Resize(Get(pipeline).sbtBuffer, tableSize);
        }

        auto getMapped = [&](u64 offset, u32 i) { return Buffer_GetMapped(Get(pipeline).sbtBuffer) + offset + (i * handleStride); };

        std::vector<u8> handles(groups.size() * handleSize);
        VkCall(vkGetRayTracingShaderGroupHandlesKHR(device, Get(pipeline).pipeline,
            0, u32(groups.size()),
            u32(handles.size()), handles.data()));

        auto getHandle = [&](u32 i) { return handles.data() + (i * handleSize); };

        // Gen

        Get(pipeline).rayGenRegion.size = handleSize;
        Get(pipeline).rayGenRegion.stride = handleSize;
        for (u32 i = 0; i < rayGenIndices.size(); ++i)
            std::memcpy(getMapped(0, i), getHandle(rayGenIndices[i]), handleSize);

        // Miss

        Get(pipeline).rayMissRegion.deviceAddress = Buffer_GetAddress(Get(pipeline).sbtBuffer) + rayMissOffset;
        Get(pipeline).rayMissRegion.size = rayHitOffset - rayMissOffset;
        Get(pipeline).rayMissRegion.stride = handleStride;
        for (u32 i = 0; i < rayMissIndices.size(); ++i)
            std::memcpy(getMapped(rayMissOffset, i), getHandle(rayMissIndices[i]), handleSize);

        // Hit

        Get(pipeline).rayHitRegion.deviceAddress = Buffer_GetAddress(Get(pipeline).sbtBuffer) + rayHitOffset;
        Get(pipeline).rayHitRegion.size = rayCallOffset - rayHitOffset;
        Get(pipeline).rayHitRegion.stride = handleStride;
        for (u32 i = 0; i < rayHitIndices.size(); ++i)
            std::memcpy(getMapped(rayHitOffset, i), getHandle(rayHitIndices[i]), handleSize);

        // Call

        Get(pipeline).rayCallRegion.deviceAddress = Buffer_GetAddress(Get(pipeline).sbtBuffer) + rayCallOffset;
        Get(pipeline).rayCallRegion.size = tableSize - rayCallOffset;
        Get(pipeline).rayCallRegion.stride = handleStride;
        for (u32 i = 0; i < rayCallIndices.size(); ++i)
            std::memcpy(getMapped(rayCallOffset, i), getHandle(rayCallIndices[i]), handleSize);
    }

    void VulkanContext::Cmd_TraceRays(CommandList cmd, RayTracingPipeline pipeline, Vec3U extent, u32 genIndex)
    {
        vkCmdBindPipeline(Get(cmd).buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, Get(pipeline).pipeline);

        auto rayGenRegion = Get(pipeline).rayGenRegion;
        rayGenRegion.deviceAddress = Buffer_GetAddress(Get(pipeline).sbtBuffer) + (rayGenRegion.stride * genIndex);

        vkCmdTraceRaysKHR(Get(cmd).buffer,
            &rayGenRegion,
            &Get(pipeline).rayMissRegion,
            &Get(pipeline).rayHitRegion,
            &Get(pipeline).rayCallRegion,
            extent.x, extent.y, extent.z);
    }
}