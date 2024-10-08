#include "VulkanRHI.hpp"

namespace nova
{
    void CommandList::BeginRendering(const RenderingInfo& info) const
    {
        NOVA_STACK_POINT();

        impl->color_attachments_formats.resize(info.color_attachments.size());

        auto color_attachment_infos = NOVA_STACK_ALLOC(VkRenderingAttachmentInfo, info.color_attachments.size());
        for (u32 i = 0; i < info.color_attachments.size(); ++i) {
            auto image = info.color_attachments[i];

            impl->Transition(info.color_attachments[i], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

            color_attachment_infos[i] = VkRenderingAttachmentInfo {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = image->view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };

            impl->color_attachments_formats[i] = info.color_attachments[i]->format;
        }

        VkRenderingInfo vk_info {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = { { info.region.offset.x, info.region.offset.y }, { info.region.extent.x, info.region.extent.y } },
            .layerCount = info.layers,
            .viewMask = info.view_mask,
            .colorAttachmentCount = u32(info.color_attachments.size()),
            .pColorAttachments = color_attachment_infos,
        };

        impl->layers = info.layers;
        impl->view_mask = info.view_mask;

        VkRenderingAttachmentInfo depth_info = {};
        VkRenderingAttachmentInfo stencil_info = {};

        if (info.depth_attachment == info.stencil_attachment) {
            if (info.depth_attachment) {
                impl->Transition(info.depth_attachment,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);

                depth_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depth_info.imageView = info.depth_attachment->view;
                depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                vk_info.pDepthAttachment = &depth_info;
                vk_info.pStencilAttachment = &depth_info;

                impl->depth_attachment_format = info.depth_attachment->format;
                impl->stencil_attachment_format = info.stencil_attachment->format;
            }
        } else {
            if (info.depth_attachment) {
                impl->Transition(info.depth_attachment,
                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);

                depth_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depth_info.imageView = info.depth_attachment->view;
                depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                vk_info.pDepthAttachment = &depth_info;

                impl->depth_attachment_format = info.depth_attachment->format;
            }

            if (info.stencil_attachment) {
                impl->Transition(info.stencil_attachment,
                    VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);

                stencil_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                stencil_info.imageView = info.stencil_attachment->view;
                stencil_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                vk_info.pStencilAttachment = &stencil_info;

                impl->stencil_attachment_format = info.stencil_attachment->format;
            }
        }

        impl->context->vkCmdBeginRendering(impl->buffer, &vk_info);
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
            impl->buffer, 1, PtrTo(VkClearAttachment {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .colorAttachment = attachment,
                .clearValue = {std::visit(Overloads {
                    [&](const Vec4&  v) { return VkClearColorValue{ .float32{ v.r, v.g, v.b, v.a }}; },
                    [&](const Vec4U& v) { return VkClearColorValue{  .uint32{ v.r, v.g, v.b, v.a }}; },
                    [&](const Vec4I& v) { return VkClearColorValue{   .int32{ v.r, v.g, v.b, v.a }}; },
                }, value)},
            }),
            1, PtrTo(VkClearRect {
                .rect = { { offset.x, offset.y }, { size.x, size.y } },
                .baseArrayLayer = 0, // TODO: Layers
                .layerCount = 1, // TODO: Layers
            }));
    }

    void CommandList::ClearDepth(f32 depth, Vec2U size, Vec2I offset) const
    {
        impl->context->vkCmdClearAttachments(
            impl->buffer, 1, PtrTo(VkClearAttachment {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .clearValue = { .depthStencil = { .depth = depth } },
            }),
            1, PtrTo(VkClearRect {
                .rect = { { offset.x, offset.y }, { size.x, size.y } },
                .baseArrayLayer = 0, // TODO: Layers
                .layerCount = 1, // TODO: Layers
            }));
    }

    void CommandList::ClearStencil(u32 value, Vec2U size, Vec2I offset) const
    {
        impl->context->vkCmdClearAttachments(
            impl->buffer, 1, PtrTo(VkClearAttachment {
                .aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT,
                .clearValue = { .depthStencil = { .stencil = value } },
            }),
            1, PtrTo(VkClearRect {
                .rect = { { offset.x, offset.y }, { size.x, size.y } },
                .baseArrayLayer = 0, // TODO: Layers
                .layerCount = 1, // TODO: Layers
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