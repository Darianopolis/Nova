#include "nova_VulkanRHI.hpp"

namespace nova
{
    AccelerationStructureBuilder AccelerationStructureBuilder::Create()
    {
        auto context = rhi::Get();

        auto impl = new Impl;

        vkh::Check(context->vkCreateQueryPool(context->device, PtrTo(VkQueryPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            .queryCount = 1,
        }), context->alloc, &impl->query_pool));

        return { impl };
    }

    static
    void EnsureGeometries(AccelerationStructureBuilder builder, u32 geometry_index)
    {
        if (geometry_index >= builder->geometries.size()) {
            builder->geometries.resize(geometry_index + 1);
            builder->ranges.resize(geometry_index + 1);
            builder->primitive_counts.resize(geometry_index + 1);
        }
    }

    void AccelerationStructureBuilder::Destroy()
    {
        auto context = rhi::Get();

        if (!impl) {
            return;
        }

        context->vkDestroyQueryPool(context->device, impl->query_pool, context->alloc);

        delete impl;
        impl = nullptr;
    }

    void AccelerationStructureBuilder::AddInstances(u32 geometry_index, u64 device_address, u32 count) const
    {
        EnsureGeometries(*this, geometry_index);

        auto& instances = impl->geometries[geometry_index];
        instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        instances.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        instances.geometry.instances = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .data = {{ device_address }},
        };

        auto& range = impl->ranges[geometry_index];
        range.primitiveCount = count;

        impl->primitive_counts[geometry_index] = count;
        impl->size_dirty = true;
    }

    void AccelerationStructureBuilder::AddTriangles(u32 geometry_index,
        u64 vertex_address, Format vertex_format, u32 vertex_stride, u32 max_vertex,
        u64 index_address, IndexType index_type, u32 triangle_count) const
    {
        EnsureGeometries(*this, geometry_index);

        auto& instances = impl->geometries[geometry_index];
        instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        instances.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        instances.geometry.triangles = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = GetVulkanFormat(vertex_format).vk_format,
            .vertexData = {{ vertex_address }},
            .vertexStride = vertex_stride,
            .maxVertex = max_vertex,
            .indexType = GetVulkanIndexType(index_type),
            .indexData = {{ index_address }},
        };

        auto& range = impl->ranges[geometry_index];
        range.primitiveCount = triangle_count;

        impl->primitive_counts[geometry_index] = triangle_count;
        impl->size_dirty = true;
    }

    void AccelerationStructureBuilder::Prepare(AccelerationStructureType _type, AccelerationStructureFlags _flags,
        u32 _geometry_count, u32 _first_geometry) const
    {
        impl->type = GetVulkanAccelStructureType(_type);
        impl->flags = GetVulkanAccelStructureBuildFlags(_flags);
        impl->geometry_count = _geometry_count;
        impl->first_geometry = _first_geometry;
        impl->size_dirty = true;
    }

    u32 AccelerationStructureBuilder::InstanceSize() const
    {
        return u32(sizeof(VkAccelerationStructureInstanceKHR));
    }

    void AccelerationStructureBuilder::WriteInstance(
        void* buffer_address, u32 index,
        HAccelerationStructure structure,
        const Mat4& M,
        u32 custom_index, u8 mask,
        u32 sbt_offset, GeometryInstanceFlags geom_flags) const
    {
        VkGeometryInstanceFlagsKHR vk_flags = 0;

        if (geom_flags >= GeometryInstanceFlags::TriangleCullCounterClockwise) {
            vk_flags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FLIP_FACING_BIT_KHR;

        } else if (!(geom_flags >= GeometryInstanceFlags::TriangleCullClockwise)) {
            vk_flags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        }

        if (geom_flags >= GeometryInstanceFlags::InstanceForceOpaque) {
            vk_flags |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
        }

        static_cast<VkAccelerationStructureInstanceKHR*>(buffer_address)[index] = {
            .transform = {
                M[0][0], M[1][0], M[2][0], M[3][0],
                M[0][1], M[1][1], M[2][1], M[3][1],
                M[0][2], M[1][2], M[2][2], M[3][2],
            },
            .instanceCustomIndex = custom_index,
            .mask = mask,
            .instanceShaderBindingTableRecordOffset = sbt_offset,
            .flags = vk_flags,
            .accelerationStructureReference = structure.Unwrap().DeviceAddress(),
        };
    }

    static
    void EnsureSizes(AccelerationStructureBuilder builder)
    {
        auto context = rhi::Get();

        if (!builder->size_dirty) {
            return;
        }

        VkAccelerationStructureBuildSizesInfoKHR sizes { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };

        context->vkGetAccelerationStructureBuildSizesKHR(
            context->device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            PtrTo(VkAccelerationStructureBuildGeometryInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = builder->type,
                .flags = builder->flags,
                .geometryCount = builder->geometry_count,
                .pGeometries = builder->geometries.data() + builder->first_geometry,
            }), builder->primitive_counts.data(), &sizes);

        builder->build_size = sizes.accelerationStructureSize;

        u64 scratch_align = context->accel_structure_properties.minAccelerationStructureScratchOffsetAlignment;
        builder->update_scratch_size = sizes.updateScratchSize + scratch_align;
        builder->build_scratch_size = sizes.buildScratchSize + scratch_align;

        builder->size_dirty = false;
    }

    u64 AccelerationStructureBuilder::BuildSize() const
    {
        EnsureSizes(*this);
        return impl->build_size;
    }

    u64 AccelerationStructureBuilder::BuildScratchSize() const
    {
        EnsureSizes(*this);
        return impl->build_scratch_size;
    }

    u64 AccelerationStructureBuilder::UpdateScratchSize() const
    {
        EnsureSizes(*this);
        return impl->update_scratch_size;
    }

    u64 AccelerationStructureBuilder::CompactSize() const
    {
        auto context = rhi::Get();

        VkDeviceSize size;
        vkh::Check(context->vkGetQueryPoolResults(context->device, impl->query_pool, 0, 1, sizeof(size), &size, sizeof(size), VK_QUERY_RESULT_64_BIT));
        return size;
    }

// -----------------------------------------------------------------------------

    AccelerationStructure AccelerationStructure::Create(u64 size, AccelerationStructureType type, HBuffer buffer, u64 offset)
    {
        auto context = rhi::Get();

        auto impl = new Impl;
        impl->own_buffer = !buffer;
        if (impl->own_buffer) {
            impl->buffer = Buffer::Create(size, nova::BufferUsage::AccelStorage, nova::BufferFlags::DeviceLocal);
        } else {
            impl->buffer = buffer;
        }

        vkh::Check(context->vkCreateAccelerationStructureKHR(context->device, PtrTo(VkAccelerationStructureCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = impl->buffer->buffer,
            .offset = offset,
            .size = impl->buffer->size,
            .type = GetVulkanAccelStructureType(type),
        }), context->alloc, &impl->structure));

        impl->address = context->vkGetAccelerationStructureDeviceAddressKHR(
            context->device,
            PtrTo(VkAccelerationStructureDeviceAddressInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .accelerationStructure = impl->structure,
            }));

        return { impl };
    }

    void AccelerationStructure::Destroy()
    {
        auto context = rhi::Get();

        if (!impl) {
            return;
        }

        context->vkDestroyAccelerationStructureKHR(context->device, impl->structure, context->alloc);
        if (impl->own_buffer) {
            impl->buffer.Destroy();
        }

        delete impl;
        impl = nullptr;
    }

    u64 AccelerationStructure::DeviceAddress() const
    {
        return impl->address;
    }

    void CommandList::BuildAccelerationStructure(HAccelerationStructureBuilder builder, HAccelerationStructure structure, HBuffer scratch) const
    {
        auto context = rhi::Get();

        bool compact = builder->flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
        if (compact) {
            context->vkCmdResetQueryPool(impl->buffer, builder->query_pool, 0, 1);
        }

        context->vkCmdBuildAccelerationStructuresKHR(
            impl->buffer,
            1, PtrTo(VkAccelerationStructureBuildGeometryInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = builder->type,
                .flags = builder->flags,
                .dstAccelerationStructure = structure->structure,
                .geometryCount = builder->geometry_count,
                .pGeometries = builder->geometries.data() + builder->first_geometry,
                .scratchData = {{ AlignUpPower2(scratch->address,
                    context->accel_structure_properties.minAccelerationStructureScratchOffsetAlignment) }},
            }), PtrTo(builder->ranges.data()));


        if (compact) {
            context->vkCmdPipelineBarrier2(impl->buffer, PtrTo(VkDependencyInfo {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = PtrTo(VkMemoryBarrier2 {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    .dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                }),
            }));

            context->vkCmdWriteAccelerationStructuresPropertiesKHR(impl->buffer,
                1, &structure->structure,
                VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                builder->query_pool, 0);
        }
    }

    void CommandList::CompactAccelerationStructure(HAccelerationStructure dst, HAccelerationStructure src) const
    {
        auto context = rhi::Get();

        context->vkCmdCopyAccelerationStructureKHR(impl->buffer, PtrTo(VkCopyAccelerationStructureInfoKHR {
            .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
            .src = src->structure,
            .dst = dst->structure,
            .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR,
        }));
    }
}