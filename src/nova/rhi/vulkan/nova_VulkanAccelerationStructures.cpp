#include "nova_VulkanRHI.hpp"

namespace nova
{
    AccelerationStructureBuilder::AccelerationStructureBuilder(HContext _context)
        : Object(_context)
    {
        VkCall(vkCreateQueryPool(context->device, Temp(VkQueryPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            .queryCount = 1,
        }), context->pAlloc, &queryPool));
    }

    static
    void EnsureGeometries(HAccelerationStructureBuilder builder, u32 geometryIndex)
    {
        if (geometryIndex >= builder->geometries.size())
        {
            builder->geometries.resize(geometryIndex + 1);
            builder->ranges.resize(geometryIndex + 1);
            builder->primitiveCounts.resize(geometryIndex + 1);
        }
    }

    AccelerationStructureBuilder::~AccelerationStructureBuilder()
    {
        vkDestroyQueryPool(context->device, queryPool, context->pAlloc);
    }

    void AccelerationStructureBuilder::SetInstances(u32 geometryIndex, u64 deviceAddress, u32 count)
    {
        EnsureGeometries(this, geometryIndex);

        auto& instances = geometries[geometryIndex];
        instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        instances.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        instances.geometry.instances = VkAccelerationStructureGeometryInstancesDataKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .data = {{ deviceAddress }},
        };

        auto& range = ranges[geometryIndex];
        range.primitiveCount = count;

        primitiveCounts[geometryIndex] = count;
    }

    void AccelerationStructureBuilder::SetTriangles(u32 geometryIndex,
        u64 vertexAddress, Format vertexFormat, u32 vertexStride, u32 maxVertex,
        u64 indexAddress, IndexType indexType, u32 triangleCount)
    {
        EnsureGeometries(this, geometryIndex);

        auto& instances = geometries[geometryIndex];
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

        auto& range = ranges[geometryIndex];
        range.primitiveCount = triangleCount;

        primitiveCounts[geometryIndex] = triangleCount;
    }

    void AccelerationStructureBuilder::Prepare(AccelerationStructureType _type, AccelerationStructureFlags _flags,
        u32 _geometryCount, u32 _firstGeometry)
    {
        type = GetVulkanAccelStructureType(_type);
        flags = GetVulkanAccelStructureBuildFlags(_flags);
        geometryCount = _geometryCount;
        firstGeometry = _firstGeometry;
        sizeDirty = true;
    }

    u32 AccelerationStructureBuilder::GetInstanceSize()
    {
        return u32(sizeof(VkAccelerationStructureInstanceKHR));
    }

    void AccelerationStructureBuilder::WriteInstance(
        void* bufferAddress, u32 index,
        HAccelerationStructure structure,
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
            .accelerationStructureReference = structure->GetAddress(),
        };
    }

    static
    void EnsureSizes(HContext ctx, HAccelerationStructureBuilder builder)
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

    u64 AccelerationStructureBuilder::GetBuildSize()
    {
        EnsureSizes(context, this);
        return buildSize;
    }

    u64 AccelerationStructureBuilder::GetBuildScratchSize()
    {
        EnsureSizes(context, this);
        return buildScratchSize;
    }

    u64 AccelerationStructureBuilder::GetUpdateScratchSize()
    {
        EnsureSizes(context, this);
        return updateScratchSize;
    }

    u64 AccelerationStructureBuilder::GetCompactSize()
    {
        VkDeviceSize size = {};
        VkCall(vkGetQueryPoolResults(context->device, queryPool, 0, 1, sizeof(size), &size, sizeof(size), 0));
        return size;
    }

    AccelerationStructure::AccelerationStructure(HContext _context, u64 size, AccelerationStructureType type, HBuffer _buffer, u64 offset)
        : Object(_context)
    {
        ownBuffer = !_buffer;
        if (ownBuffer)
        {
            buffer = new Buffer(context, size, nova::BufferUsage::AccelStorage, nova::BufferFlags::DeviceLocal);
        }
        else
        {
            buffer = _buffer;
        }

        VkCall(vkCreateAccelerationStructureKHR(context->device, Temp(VkAccelerationStructureCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = buffer->buffer,
            .offset = offset,
            .size = buffer->size,
            .type = GetVulkanAccelStructureType(type),
        }), context->pAlloc, &structure));

        address = vkGetAccelerationStructureDeviceAddressKHR(
            context->device,
            Temp(VkAccelerationStructureDeviceAddressInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .accelerationStructure = structure
            }));
    }

    AccelerationStructure::~AccelerationStructure()
    {
        vkDestroyAccelerationStructureKHR(context->device, structure, context->pAlloc);
        if (ownBuffer)
            delete buffer.get();
    }

    u64 AccelerationStructure::GetAddress()
    {
        return address;
    }

    void CommandList::BuildAccelerationStructure(HAccelerationStructureBuilder builder, HAccelerationStructure structure, HBuffer scratch)
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
                .geometryCount = builder->geometryCount,
                .pGeometries = builder->geometries.data() + builder->firstGeometry,
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
                VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                builder->queryPool, 0);
        }
    }

    void CommandList::CompactAccelerationStructure(HAccelerationStructure dst, HAccelerationStructure src)
    {
        vkCmdCopyAccelerationStructureKHR(buffer, Temp(VkCopyAccelerationStructureInfoKHR {
            .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
            .src = src->structure,
            .dst = dst->structure,
            .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR,
        }));
    }

}