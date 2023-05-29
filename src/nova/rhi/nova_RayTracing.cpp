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

        // TODO: Build flags
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

        NOVA_LOG("#### Type = {}", int(type));

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
        u64 scratchAlign = pool->context->accelStructureProperties.minAccelerationStructureScratchOffsetAlignment;

        vkCmdBuildAccelerationStructuresKHR(
            buffer,
            1, Temp(VkAccelerationStructureBuildGeometryInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = structure->type,
                .flags = structure->flags,
                .dstAccelerationStructure = structure->structure,
                .geometryCount = u32(structure->geometries.size()),
                .pGeometries = structure->geometries.data(),
                .scratchData = {{ AlignUpPower2(scratch->address, scratchAlign) }},
            }), Temp(structure->ranges.data()));
    }
}