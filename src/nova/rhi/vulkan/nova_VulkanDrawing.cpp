#include "nova_VulkanRHI.hpp"

namespace nova
{
    void CommandList::BeginRendering(Rect2D region, Span<HTexture> colorAttachments, HTexture depthAttachment, HTexture stencilAttachment) const
    {
        impl->colorAttachmentsFormats.resize(colorAttachments.size());

        auto colorAttachmentInfos = NOVA_ALLOC_STACK(VkRenderingAttachmentInfo, colorAttachments.size());
        for (u32 i = 0; i < colorAttachments.size(); ++i) {
            auto texture = colorAttachments[i];

            impl->Transition(colorAttachments[i], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

            colorAttachmentInfos[i] = VkRenderingAttachmentInfo {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = texture->view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };

            impl->colorAttachmentsFormats[i] = colorAttachments[i]->format;
        }

        VkRenderingInfo info {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = { { region.offset.x, region.offset.y }, { region.extent.x, region.extent.y } },
            .layerCount = 1,
            .colorAttachmentCount = u32(colorAttachments.size()),
            .pColorAttachments = colorAttachmentInfos,
        };

        VkRenderingAttachmentInfo depthInfo = {};
        VkRenderingAttachmentInfo stencilInfo = {};

        if (depthAttachment == stencilAttachment) {
            if (depthAttachment) {
                impl->Transition(depthAttachment,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);

                depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depthInfo.imageView = depthAttachment->view;
                depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                info.pDepthAttachment = &depthInfo;
                info.pStencilAttachment = &depthInfo;

                impl->depthAttachmentFormat = depthAttachment->format;
                impl->stencilAttachmentFormat = stencilAttachment->format;
            }
        } else {
            if (depthAttachment) {
                impl->Transition(depthAttachment,
                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);

                depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depthInfo.imageView = depthAttachment->view;
                depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                info.pDepthAttachment = &depthInfo;

                impl->depthAttachmentFormat = depthAttachment->format;
            }

            if (stencilAttachment) {
                impl->Transition(stencilAttachment,
                    VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);

                stencilInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                stencilInfo.imageView = stencilAttachment->view;
                stencilInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                info.pStencilAttachment = &stencilInfo;

                impl->stencilAttachmentFormat = stencilAttachment->format;
            }
        }

        impl->context->vkCmdBeginRendering(impl->buffer, &info);
    }

    void CommandList::EndRendering() const
    {
        impl->context->vkCmdEndRendering(impl->buffer);
    }

    void CommandList::BindIndexBuffer(HBuffer indexBuffer, IndexType indexType, u64 offset) const
    {
        impl->context->vkCmdBindIndexBuffer(impl->buffer, indexBuffer->buffer, offset, GetVulkanIndexType(indexType));
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

    void CommandList::Draw(u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDraw(impl->buffer, vertices, instances, firstVertex, firstInstance);
    }

    void CommandList::DrawIndirect(HBuffer buffer, u64 offset, u32 count, u32 stride) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDrawIndirect(impl->buffer, buffer->buffer, offset, count, stride);
    }

    void CommandList::DrawIndirectCount(HBuffer commands, u64 commandOffset, HBuffer count, u64 countOffset, u32 maxCount, u32 stride) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDrawIndirectCount(impl->buffer, commands->buffer, commandOffset, count->buffer, countOffset, maxCount, stride);
    }

// -----------------------------------------------------------------------------
//                              Draw Indexed
// -----------------------------------------------------------------------------

    void CommandList::DrawIndexed(u32 indices, u32 instances, u32 firstIndex, u32 vertexOffset, u32 firstInstance) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDrawIndexed(impl->buffer, indices, instances, firstIndex, vertexOffset, firstInstance);
    }

    void CommandList::DrawIndexedIndirect(HBuffer buffer, u64 offset, u32 count, u32 stride) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDrawIndexedIndirect(impl->buffer, buffer->buffer, offset, count, stride);
    }

    void CommandList::DrawIndexedIndirectCount(HBuffer commands, u64 commandOffset, HBuffer count, u64 countOffset, u32 maxCount, u32 stride) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDrawIndexedIndirectCount(impl->buffer, commands->buffer, commandOffset, count->buffer, countOffset, maxCount, stride);
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

    void CommandList::DrawMeshTasksIndirectCount(HBuffer commands, u64 commandOffset, HBuffer count, u64 countOffset, u32 maxCount, u32 stride) const
    {
        impl->EnsureGraphicsState();
        impl->context->vkCmdDrawMeshTasksIndirectCountEXT(impl->buffer, commands->buffer, commandOffset, count->buffer, countOffset, maxCount, stride);
    }
}