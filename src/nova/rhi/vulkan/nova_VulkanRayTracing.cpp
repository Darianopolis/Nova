#include "nova_VulkanRHI.hpp"

namespace nova
{
    RayTracingPipeline RayTracingPipeline::Create()
    {
        auto context = rhi::Get();

        auto impl = new Impl;

        impl->sbt_buffer = nova::Buffer::Create(0,
            BufferUsage::ShaderBindingTable,
            BufferFlags::DeviceLocal | BufferFlags::Mapped);

        impl->handle_size = context->ray_tracing_pipeline_properties.shaderGroupHandleSize;
        impl->handle_stride = u32(AlignUpPower2(impl->handle_size,
            context->ray_tracing_pipeline_properties.shaderGroupHandleAlignment));

        return { impl };
    }

    void RayTracingPipeline::Destroy()
    {
        auto context = rhi::Get();

        if (!impl) {
            return;
        }

        impl->sbt_buffer.Destroy();
        context->vkDestroyPipeline(context->device, impl->pipeline, context->alloc);

        delete impl;
        impl = nullptr;
    }

    void RayTracingPipeline::Update(
        HShader                    ray_gen_shader,
        Span<HShader>            ray_miss_shaders,
        Span<HitShaderGroup> ray_hit_shader_group,
        Span<HShader>            callable_shaders) const
    {
        auto context = rhi::Get();

        // Convert to stages and groups

        HashMap<VkShaderModule, u32> stage_indices;
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        u32 raygen_index;
        u32 raymiss_index_start;
        u32 raycall_index_start;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

        auto GetShaderIndex = [&](Shader shader) {
            if (!shader) {
                return VK_SHADER_UNUSED_KHR;
            }

            if (!stage_indices.contains(shader->handle)) {
                stage_indices.insert({ shader->handle, u32(stages.size()) });
                stages.push_back(shader->GetStageInfo());
            }

            return stage_indices.at(shader->handle);
        };

        auto CreateGroup = [&]() -> auto& {
            auto& info = groups.emplace_back();
            info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            info.generalShader = info.closestHitShader = info.anyHitShader = info.intersectionShader = VK_SHADER_UNUSED_KHR;
            return info;
        };

        // Ray Hit groups
        for (auto& group : ray_hit_shader_group) {
            auto& info = CreateGroup();
            info.type = group.intersection_shader
                ? VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR
                : VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            info.closestHitShader = GetShaderIndex(group.closesthit_shader);
            info.anyHitShader = GetShaderIndex(group.anyhit_shader);
            info.intersectionShader = GetShaderIndex(group.intersection_shader);
        }

        // Ray Gen group
        raygen_index = u32(groups.size());
        CreateGroup().generalShader = GetShaderIndex(ray_gen_shader);

        // Ray Miss groups
        raymiss_index_start = u32(groups.size());
        for (auto& shader : ray_miss_shaders) {
            CreateGroup().generalShader = GetShaderIndex(shader);
        }

        // Ray Call groups
        raycall_index_start = u32(groups.size());
        for (auto& shader : callable_shaders) {
            CreateGroup().generalShader = GetShaderIndex(shader);
        }

        // Create pipeline

        if (impl->pipeline) {
            context->vkDestroyPipeline(context->device, impl->pipeline, context->alloc);
        }

        vkh::Check(context->vkCreateRayTracingPipelinesKHR(context->device,
            0, context->pipeline_cache,
            1, PtrTo(VkRayTracingPipelineCreateInfoKHR {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
                .flags = context->descriptor_buffers
                    ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                    : VkPipelineCreateFlags(0),
                .stageCount = u32(stages.size()),
                .pStages = stages.data(),
                .groupCount = u32(groups.size()),
                .pGroups = groups.data(),
                .maxPipelineRayRecursionDepth = 1, // TODO: Parameterize
                .layout = context->global_heap.pipeline_layout,
            }),
            context->alloc, &impl->pipeline));

        // Compute table parameters

        u32 handle_size    = impl->handle_size;
        u32 handle_stride  = impl->handle_stride;
        u64 group_align    = context->ray_tracing_pipeline_properties.shaderGroupBaseAlignment;
        u64 raygen_offset  = AlignUpPower2(                 ray_hit_shader_group.size() * handle_stride, group_align);
        u64 raymiss_offset = AlignUpPower2(raygen_offset  +                               handle_stride, group_align);
        u64 raycall_offset = AlignUpPower2(raymiss_offset +     ray_miss_shaders.size() * handle_stride, group_align);
        u64 table_size     =               raycall_offset +     callable_shaders.size() * handle_stride;

        // Allocate table and get groups from pipeline

        if (table_size > impl->sbt_buffer.Size()) {
            if (context->config.trace) {
                Log("Resizing existing buffer");
            }
            impl->sbt_buffer.Resize(std::max(256ull, table_size));
        }

        auto GetMapped = [&](u64 offset, u32 i) {
            return impl->sbt_buffer.HostAddress() + offset + (i * handle_stride);
        };

        impl->handles.resize(groups.size() * handle_size);
        vkh::Check(context->vkGetRayTracingShaderGroupHandlesKHR(context->device, impl->pipeline,
            0, u32(groups.size()),
            u32(impl->handles.size()), impl->handles.data()));

        auto GetHandle = [&](u32 i) {
            return impl->handles.data() + (i * handle_size);
        };

        // Hit

        impl->rayhit_region.deviceAddress = impl->sbt_buffer.DeviceAddress();
        impl->rayhit_region.size = raygen_offset;
        impl->rayhit_region.stride = handle_stride;
        for (u32 i = 0; i < ray_hit_shader_group.size(); ++i) {
            std::memcpy(GetMapped(0, i), GetHandle(i), handle_size);
        }

        // Gen

        impl->raygen_region.deviceAddress = impl->sbt_buffer.DeviceAddress() + raygen_offset;
        impl->raygen_region.size = handle_stride;
        impl->raygen_region.stride = handle_stride; // raygen size === stride
        std::memcpy(GetMapped(raygen_offset, 0), GetHandle(raygen_index), handle_size);

        // Miss

        impl->raymiss_region.deviceAddress = impl->sbt_buffer.DeviceAddress() + raymiss_offset;
        impl->raymiss_region.size = raycall_offset - raymiss_offset;
        impl->raymiss_region.stride = handle_stride;
        for (u32 i = 0; i < ray_miss_shaders.size(); ++i) {
            std::memcpy(GetMapped(raymiss_offset, i), GetHandle(raymiss_index_start + i), handle_size);
        }

        // Call

        impl->raycall_region.deviceAddress = impl->sbt_buffer.DeviceAddress() + raycall_offset;
        impl->raycall_region.size = table_size - raycall_offset;
        impl->raycall_region.stride = handle_stride;
        for (u32 i = 0; i < callable_shaders.size(); ++i) {
            std::memcpy(GetMapped(raycall_offset, i), GetHandle(raycall_index_start + i), handle_size);
        }
    }

    u64 RayTracingPipeline::TableSize(u32 handles) const
    {
        return std::max(256ull, u64(handles) * impl->handle_stride);
    }

    u64 RayTracingPipeline::HandleSize() const
    {
        return impl->handle_size;
    }

    u64 RayTracingPipeline::HandleGroupAlign() const
    {
        auto context = rhi::Get();

        return context->ray_tracing_pipeline_properties.shaderGroupBaseAlignment;
    }

    u64 RayTracingPipeline::HandleStride() const
    {
        return impl->handle_stride;
    }

    HBuffer RayTracingPipeline::Handles() const
    {
        return impl->sbt_buffer;
    }

    void RayTracingPipeline::WriteHandle(void* buffer_address, u32 index, u32 group_index)
    {
        std::memcpy(
            static_cast<b8*>(buffer_address) + index * impl->handle_stride,
            impl->handles.data() + (group_index * impl->handle_stride),
            impl->handle_size);
    }

    void CommandList::TraceRays(HRayTracingPipeline pipeline, Vec3U extent, u64 hit_shader_address, u32 hit_shader_count) const
    {
        auto context = rhi::Get();

        context->vkCmdBindPipeline(impl->buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->pipeline);

        VkStridedDeviceAddressRegionKHR rayHitRegion = {};
        if (hit_shader_address) {
            rayHitRegion.deviceAddress = hit_shader_address;
            rayHitRegion.size = hit_shader_count * pipeline->rayhit_region.stride;
            rayHitRegion.stride = pipeline->rayhit_region.stride;
        } else {
            rayHitRegion = pipeline->rayhit_region;
        }

        context->vkCmdTraceRaysKHR(impl->buffer,
            &pipeline->raygen_region,
            &pipeline->raymiss_region,
            &rayHitRegion,
            &pipeline->raycall_region,
            extent.x, extent.y, extent.z);
    }
}