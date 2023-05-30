#include "nova_RHI.hpp"

#include <nova/core/nova_Math.hpp>

namespace nova
{
    AccelerationStructureBuilder* Context::CreateAccelerationStructureBuilder()
    {
        auto* builder = new AccelerationStructureBuilder;
        builder->context = this;
        NOVA_ON_SCOPE_FAILURE(&) { DestroyAccelerationStructureBuilder(builder); };

        // accelStructure->type = VkAccelerationStructureTypeKHR(type);

        // // TODO: Configurable build flags
        // accelStructure->flags = VkBuildAccelerationStructureFlagsKHR(flags);

        // if (accelStructure->flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR)
        // {
        VkCall(vkCreateQueryPool(device, Temp(VkQueryPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            .queryCount = 1,
        }), pAlloc, &builder->queryPool));
        // }

        return builder;
    }

    void Context::DestroyAccelerationStructureBuilder(AccelerationStructureBuilder* builder)
    {
        vkDestroyQueryPool(device, builder->queryPool, pAlloc);
        vkDestroyAccelerationStructureKHR(device, builder->structure, pAlloc);

        delete builder;
    }

// -----------------------------------------------------------------------------

    void AccelerationStructureBuilder::SetFlags(nova::AccelerationStructureType _type, nova::AccelerationStructureFlags _flags)
    {
        type = VkAccelerationStructureTypeKHR(_type);
        flags = VkBuildAccelerationStructureFlagsKHR(_flags);
    }

    void AccelerationStructureBuilder::ClearGeometries()
    {
        geometries.clear();
        ranges.clear();
        primitiveCounts.clear();
    }

    void AccelerationStructureBuilder::PushInstances(u64 deviceAddress, u32 count)
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

    void AccelerationStructureBuilder::PushTriangles(
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

    u64 AccelerationStructureBuilder::GetInstanceSize()
    {
        return sizeof(VkAccelerationStructureInstanceKHR);
    }

    void AccelerationStructureBuilder::WriteInstance(
        void* bufferAddress, u32 index,
        AccelerationStructure* instanceStructure,
        const Mat4& M,
        u32 customIndex, u8 mask,
        u32 sbtOffset, GeometryInstanceFlags geomFlags)
    {
        VkGeometryInstanceFlagsKHR vkFlags = 0;

        if (geomFlags >= GeometryInstanceFlags::TriangleCullCounterClockwise)
        {
            vkFlags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FLIP_FACING_BIT_KHR;
        }
        else
        if (!(geomFlags >= GeometryInstanceFlags::TriangleCullClockwise))
        {
            vkFlags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        }

        if (geomFlags >= GeometryInstanceFlags::InstanceForceOpaque)
            vkFlags |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;

        static_cast<VkAccelerationStructureInstanceKHR*>(bufferAddress)[index] = {
            .transform = {
                M[0][0], M[1][0], M[2][0], M[3][0],
                M[0][1], M[1][1], M[2][1], M[3][1],
                M[0][2], M[1][2], M[2][2], M[3][2],
            },
            .instanceCustomIndex = customIndex,
            .mask = mask,
            .instanceShaderBindingTableRecordOffset = sbtOffset,
            .flags = vkFlags,
            .accelerationStructureReference = instanceStructure->address,
        };
    }

// -----------------------------------------------------------------------------

    void AccelerationStructureBuilder::ComputeSizes()
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

        buildSize = sizes.accelerationStructureSize;

        u64 scratchAlign = context->accelStructureProperties.minAccelerationStructureScratchOffsetAlignment;
        updateScratchSize = sizes.updateScratchSize + scratchAlign;
        buildScratchSize = sizes.buildScratchSize + scratchAlign;
    }

    u64 AccelerationStructureBuilder::GetCompactSize()
    {
        VkDeviceSize size = {};
        VkCall(vkGetQueryPoolResults(context->device, queryPool, 0, 1, sizeof(size), &size, sizeof(size), 0));
        return size;
    }

    void CommandList::BuildAccelerationStructure(AccelerationStructureBuilder* builder, AccelerationStructure* structure, Buffer* scratch)
    {
        bool compact = builder->flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
        if (compact)
            vkCmdResetQueryPool(buffer, builder->queryPool, 0, 1);

        vkCmdBuildAccelerationStructuresKHR(
            buffer,
            1, Temp(VkAccelerationStructureBuildGeometryInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = builder->type,
                .flags = builder->flags,
                .dstAccelerationStructure = structure->structure,
                .geometryCount = u32(builder->geometries.size()),
                .pGeometries = builder->geometries.data(),
                .scratchData = {{ AlignUpPower2(scratch->address,
                    pool->context->accelStructureProperties.minAccelerationStructureScratchOffsetAlignment) }},
            }), Temp(builder->ranges.data()));

        if (compact)
        {
            vkCmdPipelineBarrier2(buffer, Temp(VkDependencyInfo {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = Temp(VkMemoryBarrier2 {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    .dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                })
            }));

            vkCmdWriteAccelerationStructuresPropertiesKHR(buffer,
                1, &structure->structure,
                VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, builder->queryPool, 0);
        }
    }

    void CommandList::CompactAccelerationStructure(AccelerationStructure* dst, AccelerationStructure* src)
    {
        vkCmdCopyAccelerationStructureKHR(buffer, Temp(VkCopyAccelerationStructureInfoKHR {
            .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
            .src = src->structure,
            .dst = dst->structure,
            .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR,
        }));
    }

// -----------------------------------------------------------------------------

    AccelerationStructure* Context::CreateAccelerationStructure(usz size, AccelerationStructureType type)
    {
        auto* structure = new AccelerationStructure;
        structure->context = this;
        NOVA_ON_SCOPE_FAILURE(&) { DestroyAccelerationStructure(structure); };

        structure->buffer = CreateBuffer(size,
            nova::BufferUsage::AccelStorage,
            nova::BufferFlags::DeviceLocal);

        VkCall(vkCreateAccelerationStructureKHR(device, Temp(VkAccelerationStructureCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = structure->buffer->buffer,
            .size = structure->buffer->size,
            .type = VkAccelerationStructureTypeKHR(type),
        }), pAlloc, &structure->structure));

        structure->address = vkGetAccelerationStructureDeviceAddressKHR(
            device,
            Temp(VkAccelerationStructureDeviceAddressInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .accelerationStructure = structure->structure
                }));

        return structure;
    }

    void Context::DestroyAccelerationStructure(AccelerationStructure* structure)
    {
        DestroyBuffer(structure->buffer);
        vkDestroyAccelerationStructureKHR(device, structure->structure, pAlloc);

        delete structure;
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

        if (!sbtBuffer || tableSize > sbtBuffer->size)
        {
            if (sbtBuffer)
                context->DestroyBuffer(sbtBuffer);

            sbtBuffer = context->CreateBuffer(
                std::max(256ull, tableSize),
                BufferUsage::ShaderBindingTable,
                BufferFlags::DeviceLocal | BufferFlags::CreateMapped);
        }

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