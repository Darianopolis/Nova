#include "nova_VulkanRHI.hpp"

namespace nova
{
    AccelerationStructureBuilder AccelerationStructureBuilder::Create(Context context)
    {
        auto impl = new Impl;
        impl->context = context;
        
        VkCall(vkCreateQueryPool(context->device, Temp(VkQueryPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            .queryCount = 1,
        }), context->pAlloc, &impl->queryPool));

        return { impl };
    }

    static
    void EnsureGeometries(AccelerationStructureBuilder builder, u32 geometryIndex)
    {
        if (geometryIndex >= builder->geometries.size())
        {
            builder->geometries.resize(geometryIndex + 1);
            builder->ranges.resize(geometryIndex + 1);
            builder->primitiveCounts.resize(geometryIndex + 1);
        }
    }

    void AccelerationStructureBuilder::Destroy()
    {
        vkDestroyQueryPool(impl->context->device, impl->queryPool, impl->context->pAlloc);
        
        delete impl;
        impl = nullptr;
    }

    void AccelerationStructureBuilder::SetInstances(u32 geometryIndex, u64 deviceAddress, u32 count) const
    {
        EnsureGeometries(*this, geometryIndex);

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
        EnsureGeometries(*this, geometryIndex);

        auto& instances = impl->geometries[geometryIndex];
        instances.sType =VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        instances.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        instances.geometry.triangles = VkAccelerationStructureGeometryTrianglesDataKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = GetVulkanFormat(vertexFormat),
            .vertexData = {{ vertexAddress }},
            .vertexStride = vertexStride,
            .maxVertex = maxVertex,
            .indexType = GetVulkanIndexType(indexType),
            .indexData = {{ indexAddress }},
        };

        auto& range = impl->ranges[geometryIndex];
        range.primitiveCount = triangleCount;

        impl->primitiveCounts[geometryIndex] = triangleCount;
    }

    void AccelerationStructureBuilder::Prepare(AccelerationStructureType _type, AccelerationStructureFlags _flags,
        u32 _geometryCount, u32 _firstGeometry) const
    {
        impl->type = GetVulkanAccelStructureType(_type);
        impl->flags = GetVulkanAccelStructureBuildFlags(_flags);
        impl->geometryCount = _geometryCount;
        impl->firstGeometry = _firstGeometry;
        impl->sizeDirty = true;
    }

    u32 AccelerationStructureBuilder::GetInstanceSize() const
    {
        return u32(sizeof(VkAccelerationStructureInstanceKHR));
    }

    void AccelerationStructureBuilder::WriteInstance(
        void* bufferAddress, u32 index,
        AccelerationStructure structure,
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
            .accelerationStructureReference = structure.GetAddress(),
        };
    }

    static
    void EnsureSizes(Context ctx, AccelerationStructureBuilder builder)
    {
        if (!builder->sizeDirty)
            return;

        VkAccelerationStructureBuildSizesInfoKHR sizes { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };

        vkGetAccelerationStructureBuildSizesKHR(
            ctx->device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            Temp(VkAccelerationStructureBuildGeometryInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = builder->type,
                .flags = builder->flags,
                .geometryCount = builder->geometryCount,
                .pGeometries = builder->geometries.data() + builder->firstGeometry,
            }), builder->primitiveCounts.data(), &sizes);

        builder->buildSize = sizes.accelerationStructureSize;

        u64 scratchAlign = ctx->accelStructureProperties.minAccelerationStructureScratchOffsetAlignment;
        builder->updateScratchSize = sizes.updateScratchSize + scratchAlign;
        builder->buildScratchSize = sizes.buildScratchSize + scratchAlign;

        builder->sizeDirty = false;
    }

    u64 AccelerationStructureBuilder::GetBuildSize() const
    {
        EnsureSizes(impl->context, *this);
        return impl->buildSize;
    }

    u64 AccelerationStructureBuilder::GetBuildScratchSize() const
    {
        EnsureSizes(impl->context, *this);
        return impl->buildScratchSize;
    }

    u64 AccelerationStructureBuilder::GetUpdateScratchSize() const
    {
        EnsureSizes(impl->context, *this);
        return impl->updateScratchSize;
    }

    u64 AccelerationStructureBuilder::GetCompactSize() const
    {
        VkDeviceSize size;
        VkCall(vkGetQueryPoolResults(impl->context->device, impl->queryPool, 0, 1, sizeof(size), &size, sizeof(size), VK_QUERY_RESULT_64_BIT));
        return size;
    }

// -----------------------------------------------------------------------------

    AccelerationStructure AccelerationStructure::Create(Context context, u64 size, AccelerationStructureType type, Buffer buffer, u64 offset)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->ownBuffer = !buffer;
        if (impl->ownBuffer)
        {
            impl->buffer = Buffer::Create(context, size, nova::BufferUsage::AccelStorage, nova::BufferFlags::DeviceLocal);
        }
        else
        {
            impl->buffer = buffer;
        }

        VkCall(vkCreateAccelerationStructureKHR(context->device, Temp(VkAccelerationStructureCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = impl->buffer->buffer,
            .offset = offset,
            .size = impl->buffer->size,
            .type = GetVulkanAccelStructureType(type),
        }), context->pAlloc, &impl->structure));

        impl->address = vkGetAccelerationStructureDeviceAddressKHR(
            context->device,
            Temp(VkAccelerationStructureDeviceAddressInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .accelerationStructure = impl->structure
            }));
        
        return { impl };
    }

    void AccelerationStructure::Destroy()
    {
        vkDestroyAccelerationStructureKHR(impl->context->device, impl->structure, impl->context->pAlloc);
        if (impl->ownBuffer)
            impl->buffer.Destroy();
        
        delete impl;
        impl = nullptr;
    }

    u64 AccelerationStructure::GetAddress() const
    {
        return impl->address;
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
}