#include "nova_RHI_Impl.hpp"

#include <nova/core/nova_Math.hpp>

namespace nova
{
    NOVA_DEFINE_HANDLE_OPERATIONS(AccelerationStructureBuilder)

    AccelerationStructureBuilder::AccelerationStructureBuilder(Context context)
        : ImplHandle(new AccelerationStructureBuilderImpl)
    {
        impl->context = context.GetImpl();

        VkCall(vkCreateQueryPool(context->device, Temp(VkQueryPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            .queryCount = 1,
        }), context->pAlloc, &impl->queryPool));
    }

    AccelerationStructureBuilderImpl::~AccelerationStructureBuilderImpl()
    {
        if (queryPool)
            vkDestroyQueryPool(context->device, queryPool, context->pAlloc);
    }

// -----------------------------------------------------------------------------

    void AccelerationStructureBuilderImpl::EnsureGeometries(u32 geometryIndex)
    {
        if (geometryIndex >= geometries.size())
        {
            geometries.resize(geometryIndex + 1);
            ranges.resize(geometryIndex + 1);
            primitiveCounts.resize(geometryIndex = 1);
        }
    }

    void AccelerationStructureBuilder::SetInstances(u32 geometryIndex, u64 deviceAddress, u32 count) const
    {
        impl->EnsureGeometries(geometryIndex);

        auto& instances = impl->geometries[geometryIndex];
        instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        instances.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        instances.geometry.instances = VkAccelerationStructureGeometryInstancesDataKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .data = {{ deviceAddress }},
        };

        auto& range = impl->ranges[geometryIndex];
        range.primitiveCount = count;

        impl->primitiveCounts[geometryIndex] = count;
    }

    void AccelerationStructureBuilder::SetTriangles(u32 geometryIndex,
        u64 vertexAddress, Format vertexFormat, u32 vertexStride, u32 maxVertex,
        u64 indexAddress, IndexType indexType, u32 triangleCount) const
    {
        impl->EnsureGeometries(geometryIndex);

        auto& instances = impl->geometries[geometryIndex];
        instances.sType =VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        instances.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        instances.geometry.triangles = VkAccelerationStructureGeometryTrianglesDataKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VkFormat(vertexFormat),
            .vertexData = {{ vertexAddress }},
            .vertexStride = vertexStride,
            .maxVertex = maxVertex,
            .indexType = VkIndexType(indexType),
            .indexData = {{ indexAddress }},
        };

        auto& range = impl->ranges[geometryIndex];
        range.primitiveCount = triangleCount;

        impl->primitiveCounts[geometryIndex] = triangleCount;
    }

// -----------------------------------------------------------------------------

    u64 AccelerationStructureBuilder::GetInstanceSize() const
    {
        return sizeof(VkAccelerationStructureInstanceKHR);
    }

    void AccelerationStructureBuilder::WriteInstance(
        void* bufferAddress, u32 index,
        AccelerationStructure& instanceStructure,
        const Mat4& M,
        u32 customIndex, u8 mask,
        u32 sbtOffset, GeometryInstanceFlags geomFlags) const
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
            .accelerationStructureReference = instanceStructure.GetAddress(),
        };
    }

// -----------------------------------------------------------------------------

    void AccelerationStructureBuilder::Prepare(
        nova::AccelerationStructureType type, nova::AccelerationStructureFlags flags,
        u32 geometryCount, u32 firstGeometry) const
    {
        impl->type = VkAccelerationStructureTypeKHR(type);
        impl->flags = VkBuildAccelerationStructureFlagsKHR(flags);
        impl->geometryCount = geometryCount;
        impl->firstGeometry = firstGeometry;
        impl->sizeDirty = true;
    }

    void AccelerationStructureBuilderImpl::EnsureSizes()
    {
        if (!sizeDirty)
            return;

        VkAccelerationStructureBuildSizesInfoKHR sizes { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };

        vkGetAccelerationStructureBuildSizesKHR(
            context->device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            Temp(VkAccelerationStructureBuildGeometryInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = type,
                .flags = flags,
                .geometryCount = geometryCount,
                .pGeometries = geometries.data() + firstGeometry,
            }), primitiveCounts.data(), &sizes);

        buildSize = sizes.accelerationStructureSize;

        u64 scratchAlign = context->accelStructureProperties.minAccelerationStructureScratchOffsetAlignment;
        updateScratchSize = sizes.updateScratchSize + scratchAlign;
        buildScratchSize = sizes.buildScratchSize + scratchAlign;

        sizeDirty = false;
    }

    u64 AccelerationStructureBuilder::GetBuildSize() const
    {
        impl->EnsureSizes();
        return impl->buildSize;
    }

    u64 AccelerationStructureBuilder::GetBuildScratchSize() const
    {
        impl->EnsureSizes();
        return impl->buildScratchSize;
    }

    u64 AccelerationStructureBuilder::GetUpdateScratchSize() const
    {
        impl->EnsureSizes();
        return impl->updateScratchSize;
    }

    u64 AccelerationStructureBuilder::GetCompactSize() const
    {
        VkDeviceSize size = {};
        VkCall(vkGetQueryPoolResults(impl->context->device, impl->queryPool, 0, 1, sizeof(size), &size, sizeof(size), 0));
        return size;
    }

    void CommandList::BuildAccelerationStructure(AccelerationStructureBuilder builder, AccelerationStructure structure, Buffer scratch) const
    {
        bool compact = builder->flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
        if (compact)
            vkCmdResetQueryPool(impl->buffer, builder->queryPool, 0, 1);

        vkCmdBuildAccelerationStructuresKHR(
            impl->buffer,
            1, Temp(VkAccelerationStructureBuildGeometryInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = builder->type,
                .flags = builder->flags,
                .dstAccelerationStructure = structure->structure,
                .geometryCount = builder->geometryCount,
                .pGeometries = builder->geometries.data() + builder->firstGeometry,
                .scratchData = {{ AlignUpPower2(scratch->address,
                    impl->pool->context->accelStructureProperties.minAccelerationStructureScratchOffsetAlignment) }},
            }), Temp(builder->ranges.data()));

        if (compact)
        {
            vkCmdPipelineBarrier2(impl->buffer, Temp(VkDependencyInfo {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = Temp(VkMemoryBarrier2 {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    .dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                })
            }));

            vkCmdWriteAccelerationStructuresPropertiesKHR(impl->buffer,
                1, &structure->structure,
                VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                builder->queryPool, 0);
        }
    }

    void CommandList::CompactAccelerationStructure(AccelerationStructure dst, AccelerationStructure src) const
    {
        vkCmdCopyAccelerationStructureKHR(impl->buffer, Temp(VkCopyAccelerationStructureInfoKHR {
            .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
            .src = src->structure,
            .dst = dst->structure,
            .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR,
        }));
    }

// -----------------------------------------------------------------------------

    NOVA_DEFINE_HANDLE_OPERATIONS(AccelerationStructure)

    AccelerationStructure::AccelerationStructure(Context context, usz size, AccelerationStructureType type)
        : ImplHandle(new AccelerationStructureImpl)
    {
        impl->context = context.GetImpl();

        impl->buffer = Buffer(context, size,
            nova::BufferUsage::AccelStorage,
            nova::BufferFlags::DeviceLocal);

        VkCall(vkCreateAccelerationStructureKHR(context->device, Temp(VkAccelerationStructureCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = impl->buffer->buffer,
            .size = impl->buffer->size,
            .type = VkAccelerationStructureTypeKHR(type),
        }), context->pAlloc, &impl->structure));

        impl->address = vkGetAccelerationStructureDeviceAddressKHR(
            context->device,
            Temp(VkAccelerationStructureDeviceAddressInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .accelerationStructure = impl->structure
            }));
    }

    AccelerationStructureImpl::~AccelerationStructureImpl()
    {
        if (structure)
            vkDestroyAccelerationStructureKHR(context->device, structure, context->pAlloc);
    }

    u64 AccelerationStructure::GetAddress() const noexcept
    {
        return impl->address;
    }

// -----------------------------------------------------------------------------

    RayTracingPipeline::RayTracingPipeline(Context context)
        : ImplHandle(new RayTracingPipelineImpl)
    {
        impl->context = context.GetImpl();
    }

    RayTracingPipelineImpl::~RayTracingPipelineImpl()
    {
        if (pipeline)
            vkDestroyPipeline(context->device, pipeline, context->pAlloc);
    }

// -----------------------------------------------------------------------------

    NOVA_DEFINE_HANDLE_OPERATIONS(RayTracingPipeline)

    void RayTracingPipeline::Update(
            PipelineLayout layout,
            Span<Shader> rayGenShaders,
            Span<Shader> rayMissShaders,
            Span<HitShaderGroup> rayHitShaderGroup,
            Span<Shader> callableShaders) const
    {
        auto& context = impl->context;

        // Convert to stages and groups

        ankerl::unordered_dense::map<VkShaderModule, u32> stageIndices;
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        std::vector<u32> rayGenIndices, rayMissIndices, rayHitIndices, rayCallIndices;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

        auto getShaderIndex = [&](Shader shader) {
            if (!shader.IsValid())
                return VK_SHADER_UNUSED_KHR;

            if (!stageIndices.contains(shader->module))
            {
                stageIndices.insert({ shader->module, u32(stages.size()) });
                stages.push_back(shader.GetStageInfo());
            }

            return stageIndices.at(shader->module);
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
            info.type = group.intersectionShader.IsValid()
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

        if (impl->pipeline)
            vkDestroyPipeline(context->device, impl->pipeline, context->pAlloc);

        VkCall(vkCreateRayTracingPipelinesKHR(context->device, 0, nullptr, 1, Temp(VkRayTracingPipelineCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
            .flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
            .stageCount = u32(stages.size()),
            .pStages = stages.data(),
            .groupCount = u32(groups.size()),
            .pGroups = groups.data(),
            .maxPipelineRayRecursionDepth = 2, // TODO: Parameterize
            .layout = layout->layout,
        }), context->pAlloc, &impl->pipeline));

        // Compute table parameters

        u32 handleSize = context->rayTracingPipelineProperties.shaderGroupHandleSize;
        u32 handleStride = u32(AlignUpPower2(handleSize, context->rayTracingPipelineProperties.shaderGroupHandleAlignment));
        u64 groupAlign = context->rayTracingPipelineProperties.shaderGroupBaseAlignment;
        u64 rayMissOffset = AlignUpPower2(rayGenIndices.size() * handleStride, groupAlign);
        u64 rayHitOffset = rayMissOffset + AlignUpPower2(rayMissIndices.size() * handleSize, groupAlign);
        u64 rayCallOffset = rayHitOffset + AlignUpPower2(rayHitIndices.size() * handleSize, groupAlign);
        u64 tableSize = rayCallOffset + rayCallIndices.size() * handleStride;

        // Allocate table and get groups from pipeline

        if (!impl->sbtBuffer.IsValid() || tableSize > impl->sbtBuffer->size)
        {
            impl->sbtBuffer = Buffer(context,
                std::max(256ull, tableSize),
                BufferUsage::ShaderBindingTable,
                BufferFlags::DeviceLocal | BufferFlags::CreateMapped);
        }

        auto getMapped = [&](u64 offset, u32 i) { return impl->sbtBuffer->mapped + offset + (i * handleStride); };

        std::vector<u8> handles(groups.size() * handleSize);
        VkCall(vkGetRayTracingShaderGroupHandlesKHR(context->device, impl->pipeline,
            0, u32(groups.size()),
            u32(handles.size()), handles.data()));

        auto getHandle = [&](u32 i) { return handles.data() + (i * handleSize); };

        // Gen

        impl->rayGenRegion.size = handleSize;
        impl->rayGenRegion.stride = handleSize;
        for (u32 i = 0; i < rayGenIndices.size(); ++i)
            std::memcpy(getMapped(0, i), getHandle(rayGenIndices[i]), handleSize);

        // Miss

        impl->rayMissRegion.deviceAddress = impl->sbtBuffer->address + rayMissOffset;
        impl->rayMissRegion.size = rayHitOffset - rayMissOffset;
        impl->rayMissRegion.stride = handleStride;
        for (u32 i = 0; i < rayMissIndices.size(); ++i)
            std::memcpy(getMapped(rayMissOffset, i), getHandle(rayMissIndices[i]), handleSize);

        // Hit

        impl->rayHitRegion.deviceAddress = impl->sbtBuffer->address + rayHitOffset;
        impl->rayHitRegion.size = rayCallOffset - rayHitOffset;
        impl->rayHitRegion.stride = handleStride;
        for (u32 i = 0; i < rayHitIndices.size(); ++i)
            std::memcpy(getMapped(rayHitOffset, i), getHandle(rayHitIndices[i]), handleSize);

        // Call

        impl->rayCallRegion.deviceAddress = impl->sbtBuffer->address + rayCallOffset;
        impl->rayCallRegion.size = tableSize - rayCallOffset;
        impl->rayCallRegion.stride = handleStride;
        for (u32 i = 0; i < rayCallIndices.size(); ++i)
            std::memcpy(getMapped(rayCallOffset, i), getHandle(rayCallIndices[i]), handleSize);
    }

    void CommandList::BindPipeline(RayTracingPipeline pipeline) const
    {
        vkCmdBindPipeline(impl->buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->pipeline);
    }

    void CommandList::TraceRays(RayTracingPipeline pipeline, Vec3U extent, u32 genIndex) const
    {
        auto rayGenRegion = pipeline->rayGenRegion;
        rayGenRegion.deviceAddress = pipeline->sbtBuffer->address + (rayGenRegion.stride * genIndex);

        vkCmdTraceRaysKHR(impl->buffer,
            &rayGenRegion,
            &pipeline->rayMissRegion,
            &pipeline->rayHitRegion,
            &pipeline->rayCallRegion,
            extent.x, extent.y, extent.z);
    }
}