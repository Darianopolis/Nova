#include "nova_VulkanRHI.hpp"

namespace nova
{
    AccelerationStructureBuilder VulkanContext::AccelerationStructures_CreateBuilder()
    {
        auto[id, as] = accelerationStructureBuilders.Acquire();

        VkCall(vkCreateQueryPool(device, Temp(VkQueryPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            .queryCount = 1,
        }), pAlloc, &as.queryPool));

        return id;
    }

    static
    void EnsureGeomtries(VulkanAccelerationStructureBuilder& builder, u32 geometryIndex)
    {
        if (geometryIndex >= builder.geometries.size())
        {
            builder.geometries.resize(geometryIndex + 1);
            builder.ranges.resize(geometryIndex + 1);
            builder.primitiveCounts.resize(geometryIndex + 1);
        }
    }

    void VulkanContext::AccelerationStructures_DestroyBuilder(AccelerationStructureBuilder builder)
    {
        vkDestroyQueryPool(device, Get(builder).queryPool, pAlloc);
        accelerationStructureBuilders.Return(builder);
    }

    void VulkanContext::AccelerationStructures_SetInstances(AccelerationStructureBuilder builder, u32 geometryIndex, u64 deviceAddress, u32 count)
    {
        EnsureGeomtries(Get(builder), geometryIndex);

        auto& instances = Get(builder).geometries[geometryIndex];
        instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        instances.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        instances.geometry.instances = VkAccelerationStructureGeometryInstancesDataKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .data = {{ deviceAddress }},
        };

        auto& range = Get(builder).ranges[geometryIndex];
        range.primitiveCount = count;

        Get(builder).primitiveCounts[geometryIndex] = count;
    }

    void VulkanContext::AccelerationStructures_SetTriangles(AccelerationStructureBuilder builder, u32 geometryIndex,
        u64 vertexAddress, Format vertexFormat, u32 vertexStride, u32 maxVertex,
        u64 indexAddress, IndexType indexType, u32 triangleCount)
    {
        EnsureGeomtries(Get(builder), geometryIndex);

        auto& instances = Get(builder).geometries[geometryIndex];
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

        auto& range = Get(builder).ranges[geometryIndex];
        range.primitiveCount = triangleCount;

        Get(builder).primitiveCounts[geometryIndex] = triangleCount;
    }

    void VulkanContext::AccelerationStructures_Prepare(AccelerationStructureBuilder builder, AccelerationStructureType type, AccelerationStructureFlags flags,
        u32 geometryCount, u32 firstGeometry)
    {
        Get(builder).type = VkAccelerationStructureTypeKHR(type);
        Get(builder).flags = VkBuildAccelerationStructureFlagsKHR(flags);
        Get(builder).geometryCount = geometryCount;
        Get(builder).firstGeometry = firstGeometry;
        Get(builder).sizeDirty = true;
    }

    u32 VulkanContext::AccelerationStructures_GetInstanceSize()
    {
        return u32(sizeof(VkAccelerationStructureInstanceKHR));
    }

    void VulkanContext::AccelerationStructures_WriteInstance(
        void* bufferAddress, u32 index,
        AccelerationStructure structure,
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
            .accelerationStructureReference = AccelerationStructures_GetAddress(structure),
        };
    }

    static
    void EnsureSizes(VulkanContext& ctx, VulkanAccelerationStructureBuilder& builder)
    {
        if (!builder.sizeDirty)
            return;

        VkAccelerationStructureBuildSizesInfoKHR sizes { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };

        vkGetAccelerationStructureBuildSizesKHR(
            ctx.device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            Temp(VkAccelerationStructureBuildGeometryInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = builder.type,
                .flags = builder.flags,
                .geometryCount = builder.geometryCount,
                .pGeometries = builder.geometries.data() + builder.firstGeometry,
            }), builder.primitiveCounts.data(), &sizes);

        builder.buildSize = sizes.accelerationStructureSize;

        u64 scratchAlign = ctx.accelStructureProperties.minAccelerationStructureScratchOffsetAlignment;
        builder.updateScratchSize = sizes.updateScratchSize + scratchAlign;
        builder.buildScratchSize = sizes.buildScratchSize + scratchAlign;

        builder.sizeDirty = false;
    }

    u64 VulkanContext::AccelerationStructures_GetBuildSize(AccelerationStructureBuilder builder)
    {
        EnsureSizes(*this, Get(builder));
        return Get(builder).buildSize;
    }

    u64 VulkanContext::AccelerationStructures_GetBuildScratchSize(AccelerationStructureBuilder builder)
    {
        EnsureSizes(*this, Get(builder));
        return Get(builder).buildScratchSize;
    }

    u64 VulkanContext::AccelerationStructures_GetUpdateScratchSize(AccelerationStructureBuilder builder)
    {
        EnsureSizes(*this, Get(builder));
        return Get(builder).updateScratchSize;
    }

    u64 VulkanContext::AccelerationStructures_GetCompactSize(AccelerationStructureBuilder builder)
    {
        VkDeviceSize size = {};
        VkCall(vkGetQueryPoolResults(device, Get(builder).queryPool, 0, 1, sizeof(size), &size, sizeof(size), 0));
        return size;
    }

    AccelerationStructure VulkanContext::AccelerationStructures_Create(u64 size, AccelerationStructureType type)
    {
        auto[id, set] = accelerationStructures.Acquire();

        set.buffer = Buffer_Create(size, nova::BufferUsage::AccelStorage, nova::BufferFlags::DeviceLocal);

        VkCall(vkCreateAccelerationStructureKHR(device, Temp(VkAccelerationStructureCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = Get(set.buffer).buffer,
            .size = Get(set.buffer).size,
            .type = VkAccelerationStructureTypeKHR(type),
        }), pAlloc, &set.structure));

        set.address = vkGetAccelerationStructureDeviceAddressKHR(
            device,
            Temp(VkAccelerationStructureDeviceAddressInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .accelerationStructure = set.structure
            }));

        return id;
    }

    void VulkanContext::AccelerationStructures_Destroy(AccelerationStructure set)
    {
        vkDestroyAccelerationStructureKHR(device, Get(set).structure, pAlloc);
        Buffer_Destroy(Get(set).buffer);
        accelerationStructures.Return(set);
    }

    u64 VulkanContext::AccelerationStructures_GetAddress(AccelerationStructure set)
    {
        return Get(set).address;
    }

    void VulkanContext::Cmd_BuildAccelerationStructure(CommandList cmd, AccelerationStructureBuilder builder, AccelerationStructure structure, Buffer scratch)
    {
        bool compact = Get(builder).flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
        if (compact)
            vkCmdResetQueryPool(Get(cmd).buffer, Get(builder).queryPool, 0, 1);

        vkCmdBuildAccelerationStructuresKHR(
            Get(cmd).buffer,
            1, Temp(VkAccelerationStructureBuildGeometryInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = Get(builder).type,
                .flags = Get(builder).flags,
                .dstAccelerationStructure = Get(structure).structure,
                .geometryCount = Get(builder).geometryCount,
                .pGeometries = Get(builder).geometries.data() + Get(builder).firstGeometry,
                .scratchData = {{ AlignUpPower2(Get(scratch).address,
                    accelStructureProperties.minAccelerationStructureScratchOffsetAlignment) }},
            }), Temp(Get(builder).ranges.data()));

        if (compact)
        {
            vkCmdPipelineBarrier2(Get(cmd).buffer, Temp(VkDependencyInfo {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = Temp(VkMemoryBarrier2 {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    .dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                })
            }));

            vkCmdWriteAccelerationStructuresPropertiesKHR(Get(cmd).buffer,
                1, &Get(structure).structure,
                VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                Get(builder).queryPool, 0);
        }
    }

    void VulkanContext::Cmd_CompactAccelerationStructure(CommandList cmd, AccelerationStructure dst, AccelerationStructure src)
    {
        vkCmdCopyAccelerationStructureKHR(Get(cmd).buffer, Temp(VkCopyAccelerationStructureInfoKHR {
            .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
            .src = Get(src).structure,
            .dst = Get(dst).structure,
            .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR,
        }));
    }

}