#include "nova_VulkanRHI.hpp"

namespace nova
{
    void CommandList::BeginRendering(Rect2D region, Span<HTexture> color_attachments, HTexture depth_attachment, HTexture stencil_attachment) const
    {
        impl->color_attachments_formats.resize(color_attachments.size());

        auto color_attachment_infos = NOVA_ALLOC_STACK(VkRenderingAttachmentInfo, color_attachments.size());
        for (u32 i = 0; i < color_attachments.size(); ++i) {
            auto texture = color_attachments[i];

            impl->Transition(color_attachments[i], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

            color_attachment_infos[i] = VkRenderingAttachmentInfo {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = texture->view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };

            impl->color_attachments_formats[i] = color_attachments[i]->format;
        }

        VkRenderingInfo info {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = { { region.offset.x, region.offset.y }, { region.extent.x, region.extent.y } },
            .layerCount = 1,
            .colorAttachmentCount = u32(color_attachments.size()),
            .pColorAttachments = color_attachment_infos,
        };

        VkRenderingAttachmentInfo depth_info = {};
        VkRenderingAttachmentInfo stencil_info = {};

        if (depth_attachment == stencil_attachment) {
            if (depth_attachment) {
                impl->Transition(depth_attachment,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);

                depth_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depth_info.imageView = depth_attachment->view;
                depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                info.pDepthAttachment = &depth_info;
                info.pStencilAttachment = &depth_info;

                impl->depth_attachment_format = depth_attachment->format;
                impl->stencil_attachment_format = stencil_attachment->format;
            }
        } else {
            if (depth_attachment) {
                impl->Transition(depth_attachment,
                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);

                depth_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depth_info.imageView = depth_attachment->view;
                depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                info.pDepthAttachment = &depth_info;

                impl->depth_attachment_format = depth_attachment->format;
            }

            if (stencil_attachment) {
                impl->Transition(stencil_attachment,
                    VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);

                stencil_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                stencil_info.imageView = stencil_attachment->view;
                stencil_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                info.pStencilAttachment = &stencil_info;

                impl->stencil_attachment_format = stencil_attachment->format;
            }
        }

        impl->context->vkCmdBeginRendering(impl->buffer, &info);
    }

    void CommandList::EndRendering() const
    {
        impl->context->vkCmdEndRendering(impl->buffer);
    }

    void CommandList::BindIndexBuffer(HBuffer index_buffer, IndexType index_type, u64 offset) const
    {
        impl->context->vkCmdBindIndexBuffer(impl->buffer, index_buffer->buffer, offset, GetVulkanIndexType(index_type));
    }

    void CommandList::ClearColor(u32 attachment, std::variant<Vec4, Vec4U, Vec4I> value, Vec2U size, Vec2I offset) const
    {
        impl->context->vkCmdClearAttachments(
            impl->buffer, 1, nova::Temp(VkClearAttachment {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .colorAttachment = attachment,
                .clearValue = {std::visit(Overloads {
                    [&](const Vec4&  v) { return VkClearColorValue{ .float32{ v.r, v.g, v.b, v.a }}; },
                    [&](const Vec4U& v) { return VkClearColorValue{  .uint32{ v.r, v.g, v.b, v.a }}; },
                    [&](const Vec4I& v) { return VkClearColorValue{   .int32{ v.r, v.g, v.b, v.a }}; },
                }, value)},
            }),
            1, nova::Temp(VkClearRect {
                .rect = { { offset.x, offset.y }, { size.x, size.y } },
                .baseArrayLayer = 0,
                .layerCount = 1,
            }));
    }

    void CommandList::ClearDepth(f32 depth, Vec2U size, Vec2I offset) const
    {
        impl->context->vkCmdClearAttachments(
            impl->buffer, 1, nova::Temp(VkClearAttachment {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .clearValue = { .depthStencil = { .depth = depth } },
            }),
            1, nova::Temp(VkClearRect {
                .rect = { { offset.x, offset.y }, { size.x, size.y } },
                .baseArrayLayer = 0,
                .layerCount = 1,
            }));
    }

    void CommandList::ClearStencil(u32 value, Vec2U size, Vec2I offset) const
    {
        impl->context->vkCmdClearAttachments(
            impl->buffer, 1, nova::Temp(VkClearAttachment {
                .aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT,
                .clearValue = { .depthStencil = { .stencil = value } },
            }),
            1, nova::Temp(VkClearRect {
                .rect = { { offset.x, offset.y }, { size.x, size.y } },
                .baseArrayLayer = 0,
                .layerCount = 1,
            }));
    }

// -----------------------------------------------------------------------------
//                                  Draw
// -----------------------------------------------------------------------------

    void CommandList::Draw(u32 vertices, u32 instances, u32 first_vertex, u32 first_instance) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDraw(impl->buffer, vertices, instances, first_vertex, first_instance);
    }

    void CommandList::DrawIndirect(HBuffer buffer, u64 offset, u32 count, u32 stride) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDrawIndirect(impl->buffer, buffer->buffer, offset, count, stride);
    }

    void CommandList::DrawIndirectCount(HBuffer commands, u64 command_offset, HBuffer count, u64 count_offset, u32 max_count, u32 stride) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDrawIndirectCount(impl->buffer, commands->buffer, command_offset, count->buffer, count_offset, max_count, stride);
    }

// -----------------------------------------------------------------------------
//                              Draw Indexed
// -----------------------------------------------------------------------------

    void CommandList::DrawIndexed(u32 indices, u32 instances, u32 first_index, u32 vertex_offset, u32 first_instance) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDrawIndexed(impl->buffer, indices, instances, first_index, vertex_offset, first_instance);
    }

    void CommandList::DrawIndexedIndirect(HBuffer buffer, u64 offset, u32 count, u32 stride) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDrawIndexedIndirect(impl->buffer, buffer->buffer, offset, count, stride);
    }

    void CommandList::DrawIndexedIndirectCount(HBuffer commands, u64 command_offset, HBuffer count, u64 count_offset, u32 max_count, u32 stride) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDrawIndexedIndirectCount(impl->buffer, commands->buffer, command_offset, count->buffer, count_offset, max_count, stride);
    }

// -----------------------------------------------------------------------------
//                             Draw Mesh Tasks
// -----------------------------------------------------------------------------

    void CommandList::DrawMeshTasks(Vec3U groups) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDrawMeshTasksEXT(impl->buffer, groups.x, groups.y, groups.z);
    }

    void CommandList::DrawMeshTasksIndirect(HBuffer buffer, u64 offset, u32 count, u32 stride) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDrawMeshTasksIndirectEXT(impl->buffer, buffer->buffer, offset, count, stride);
    }

    void CommandList::DrawMeshTasksIndirectCount(HBuffer commands, u64 command_offset, HBuffer count, u64 count_offset, u32 max_count, u32 stride) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDrawMeshTasksIndirectCountEXT(impl->buffer, commands->buffer, command_offset, count->buffer, count_offset, max_count, stride);
    }
}