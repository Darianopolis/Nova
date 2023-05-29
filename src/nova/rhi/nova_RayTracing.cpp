#include "nova_RHI.hpp"

#include <nova/core/nova_Math.hpp>

namespace nova
{
    AccelerationStructure* Context::CreateAccelerationStructure(AccelerationStructureType type)
    {
        auto* accelStructure = new AccelerationStructure;
        accelStructure->context = this;
        NOVA_ON_SCOPE_FAILURE(&) { DestroyAccelerationStructure(accelStructure); };

        accelStructure->type = VkAccelerationStructureTypeKHR(type);

        // TODO: Configurable build flags
        accelStructure->flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
            | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR;

        return accelStructure;
    }

    void Context::DestroyAccelerationStructure(AccelerationStructure* accelStructure)
    {
        vkDestroyAccelerationStructureKHR(device, accelStructure->structure, pAlloc);
        DestroyBuffer(accelStructure->buffer);

        delete accelStructure;
    }

// -----------------------------------------------------------------------------

    void AccelerationStructure::ClearGeometries()
    {
        geometries.clear();
        ranges.clear();
    }

    void AccelerationStructure::PushInstances(u64 deviceAddress, u32 count)
    {
        auto& instances = geometries.emplace_back();
        instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        instances.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        instances.geometry.instances = VkAccelerationStructureGeometryInstancesDataKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .data = {{ deviceAddress }},
        };

        auto& range = ranges.emplace_back();
        range.primitiveCount = count;

        primitiveCounts.emplace_back(count);
    }

    void AccelerationStructure::PushTriangles(
        u64 vertexAddress, VkFormat vertexFormat, u32 vertexStride, u32 maxVertex,
        u64 indexAddress, VkIndexType indexType, u32 triangleCount)
    {
        auto& instances = geometries.emplace_back();
        instances.sType =VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        instances.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        instances.geometry.triangles = VkAccelerationStructureGeometryTrianglesDataKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = vertexFormat,
            .vertexData = {{ vertexAddress }},
            .vertexStride = vertexStride,
            .maxVertex = maxVertex,
            .indexType = indexType,
            .indexData = {{ indexAddress }},
        };

        auto& range = ranges.emplace_back();
        range.primitiveCount = triangleCount;

        primitiveCounts.emplace_back(triangleCount);
    }

// -----------------------------------------------------------------------------

    bool AccelerationStructure::Resize()
    {
        VkAccelerationStructureBuildSizesInfoKHR sizes { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };

        vkGetAccelerationStructureBuildSizesKHR(
            context->device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            Temp(VkAccelerationStructureBuildGeometryInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = type,
                .flags = flags,
                .geometryCount = u32(geometries.size()),
                .pGeometries = geometries.data(),
            }), primitiveCounts.data(), &sizes);

        u64 scratchAlign = context->accelStructureProperties.minAccelerationStructureScratchOffsetAlignment;
        updateScratchSize = sizes.updateScratchSize + scratchAlign;
        buildScratchSize = sizes.buildScratchSize + scratchAlign;

        if (!buffer || sizes.accelerationStructureSize > buffer->size)
        {
            if (buffer)
                context->DestroyBuffer(buffer);

            if (structure)
                vkDestroyAccelerationStructureKHR(context->device, structure, context->pAlloc);

            buffer = context->CreateBuffer(sizes.accelerationStructureSize,
                nova::BufferUsage::AccelStorage,
                nova::BufferFlags::DeviceLocal);

            VkCall(vkCreateAccelerationStructureKHR(context->device, Temp(VkAccelerationStructureCreateInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                .buffer = buffer->buffer,
                .size = buffer->size,
                .type = type,
            }), context->pAlloc, &structure));

            address = vkGetAccelerationStructureDeviceAddressKHR(
                context->device,
                Temp(VkAccelerationStructureDeviceAddressInfoKHR {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                    .accelerationStructure = structure
                }));

            return true;
        }

        return false;
    }

    void CommandList::BuildAccelerationStructure(AccelerationStructure* structure, Buffer* scratch)
    {
        vkCmdBuildAccelerationStructuresKHR(
            buffer,
            1, Temp(VkAccelerationStructureBuildGeometryInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = structure->type,
                .flags = structure->flags,
                .dstAccelerationStructure = structure->structure,
                .geometryCount = u32(structure->geometries.size()),
                .pGeometries = structure->geometries.data(),
                .scratchData = {{ AlignUpPower2(scratch->address,
                    pool->context->accelStructureProperties.minAccelerationStructureScratchOffsetAlignment) }},
            }), Temp(structure->ranges.data()));
    }

// -----------------------------------------------------------------------------

    RayTracingPipeline* Context::CreateRayTracingPipeline()
    {
        auto* pipeline = new RayTracingPipeline;
        pipeline->context = this;
        return pipeline;
    }

    void Context::DestroyRayTracingPipeline(RayTracingPipeline* pipeline)
    {
        DestroyBuffer(pipeline->sbtBuffer);
        vkDestroyPipeline(device, pipeline->pipeline, pAlloc);

        delete pipeline;
    }

// -----------------------------------------------------------------------------

    void RayTracingPipeline::Update(
            PipelineLayout* layout,
            Span<Shader*> rayGenShaders,
            Span<Shader*> rayMissShaders,
            Span<HitShaderGroup> rayHitShaderGroup,
            Span<Shader*> callableShaders)
    {
        // Convert to stages and groups

        ankerl::unordered_dense::map<VkShaderModule, u32> stageIndices;
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        std::vector<u32> rayGenIndices, rayMissIndices, rayHitIndices, rayCallIndices;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

        auto getShaderIndex = [&](Shader* shader) {
            if (!shader)
                return VK_SHADER_UNUSED_KHR;

            if (!stageIndices.contains(shader->info.module))
            {
                stageIndices.insert({ shader->info.module, u32(stages.size()) });
                stages.push_back(shader->info);
            }

            return stageIndices.at(shader->info.module);
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

        // Debug

        NOVA_LOG("---- stages ----");
        for (auto& stage : stages)
            NOVA_LOG(" - stage.module = {} -> {}", (void*)stage.module, stageIndices.at(stage.module));
        NOVA_LOG("---- groups ----");
        for (auto& group : groups)
            NOVA_LOG(" - group.type = {}, general = {}, closest hit = {}, any hit = {}",
                (i32)group.type, group.generalShader, group.closestHitShader, group.anyHitShader);
        NOVA_LOG("---- hit group indices ----");
        for (auto& index : rayHitIndices)
            NOVA_LOG(" - index = {}", index);
        NOVA_LOG("----");

        // Create pipeline

        if (pipeline)
            vkDestroyPipeline(context->device, pipeline, context->pAlloc);

        VkCall(vkCreateRayTracingPipelinesKHR(context->device, 0, nullptr, 1, Temp(VkRayTracingPipelineCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
            .flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
            .stageCount = u32(stages.size()),
            .pStages = stages.data(),
            .groupCount = u32(groups.size()),
            .pGroups = groups.data(),
            .maxPipelineRayRecursionDepth = 2, // TODO: Parameterize
            .layout = layout->layout,
        }), context->pAlloc, &pipeline));

        // Compute table parameters

        u32 handleSize = context->rayTracingPipelineProperties.shaderGroupHandleSize;
        u32 handleStride = u32(AlignUpPower2(handleSize, context->rayTracingPipelineProperties.shaderGroupHandleAlignment));
        u64 groupAlign = context->rayTracingPipelineProperties.shaderGroupBaseAlignment;
        u64 rayMissOffset = AlignUpPower2(rayGenIndices.size() * handleStride, groupAlign);
        u64 rayHitOffset = rayMissOffset + AlignUpPower2(rayMissIndices.size() * handleSize, groupAlign);
        u64 rayCallOffset = rayHitOffset + AlignUpPower2(rayHitIndices.size() * handleSize, groupAlign);
        u64 tableSize = rayCallOffset + rayCallIndices.size() * handleStride;

        // Allocate table and get groups from pipeline

        if (sbtBuffer)
            context->DestroyBuffer(sbtBuffer);

        sbtBuffer = context->CreateBuffer(
            std::max(256ull, tableSize),
            BufferUsage::ShaderBindingTable,
            BufferFlags::DeviceLocal | BufferFlags::CreateMapped);

        auto getMapped = [&](u64 offset, u32 i) { return sbtBuffer->mapped + offset + (i * handleStride); };

        std::vector<u8> handles(groups.size() * handleSize);
        VkCall(vkGetRayTracingShaderGroupHandlesKHR(context->device, pipeline,
            0, u32(groups.size()),
            u32(handles.size()), handles.data()));

        auto getHandle = [&](u32 i) { return handles.data() + (i * handleSize); };

        // Gen

        rayGenRegion.size = handleSize;
        rayGenRegion.stride = handleSize;
        for (u32 i = 0; i < rayGenIndices.size(); ++i)
            std::memcpy(getMapped(0, i), getHandle(rayGenIndices[i]), handleSize);

        // Miss

        rayMissRegion.deviceAddress = sbtBuffer->address + rayMissOffset;
        rayMissRegion.size = rayHitOffset - rayMissOffset;
        rayMissRegion.stride = handleStride;
        for (u32 i = 0; i < rayMissIndices.size(); ++i)
            std::memcpy(getMapped(rayMissOffset, i), getHandle(rayMissIndices[i]), handleSize);

        // Hit

        rayHitRegion.deviceAddress = sbtBuffer->address + rayHitOffset;
        rayHitRegion.size = rayCallOffset - rayHitOffset;
        rayHitRegion.stride = handleStride;
        for (u32 i = 0; i < rayHitIndices.size(); ++i)
            std::memcpy(getMapped(rayHitOffset, i), getHandle(rayHitIndices[i]), handleSize);

        // Call

        rayCallRegion.deviceAddress = sbtBuffer->address + rayCallOffset;
        rayCallRegion.size = tableSize - rayCallOffset;
        rayCallRegion.stride = handleStride;
        for (u32 i = 0; i < rayCallIndices.size(); ++i)
            std::memcpy(getMapped(rayCallOffset, i), getHandle(rayCallIndices[i]), handleSize);
    }

    void CommandList::BindPipeline(RayTracingPipeline* pipeline)
    {
        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->pipeline);
    }

    void CommandList::TraceRays(RayTracingPipeline* pipeline, Vec3U extent, u32 genIndex)
    {
        pipeline->rayGenRegion.deviceAddress = pipeline->sbtBuffer->address
            + (pipeline->rayGenRegion.stride * genIndex);

        vkCmdTraceRaysKHR(buffer,
            &pipeline->rayGenRegion,
            &pipeline->rayMissRegion,
            &pipeline->rayHitRegion,
            &pipeline->rayCallRegion,
            extent.x, extent.y, extent.z);
    }
}