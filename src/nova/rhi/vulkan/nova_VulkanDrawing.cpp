#include "nova_VulkanRHI.hpp"

namespace nova
{
    void CommandList::BeginRendering(Rect2D region, Span<HTexture> colorAttachments, HTexture depthAttachment, HTexture stencilAttachment) const
    {
        impl->state->colorAttachmentsFormats.resize(colorAttachments.size());

        auto colorAttachmentInfos = NOVA_ALLOC_STACK(VkRenderingAttachmentInfo, colorAttachments.size());
        for (u32 i = 0; i < colorAttachments.size(); ++i) {
            auto texture = colorAttachments[i];

            Transition(colorAttachments[i], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);

            colorAttachmentInfos[i] = VkRenderingAttachmentInfo {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = texture->view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };

            impl->state->colorAttachmentsFormats[i] = colorAttachments[i]->format;
        }

        impl->state->renderingExtent = region.extent;

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
                Transition(depthAttachment,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR);

                depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depthInfo.imageView = depthAttachment->view;
                depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                info.pDepthAttachment = &depthInfo;
                info.pStencilAttachment = &depthInfo;

                impl->state->depthAttachmentFormat = depthAttachment->format;
                impl->state->stencilAttachmentFormat = stencilAttachment->format;
            }
        } else {
            if (depthAttachment) {
                Transition(depthAttachment,
                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR);

                depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depthInfo.imageView = depthAttachment->view;
                depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                info.pDepthAttachment = &depthInfo;

                impl->state->depthAttachmentFormat = depthAttachment->format;
            }

            if (stencilAttachment) {
                Transition(stencilAttachment,
                    VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR);

                stencilInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                stencilInfo.imageView = stencilAttachment->view;
                stencilInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                info.pStencilAttachment = &stencilInfo;

                impl->state->stencilAttachmentFormat = stencilAttachment->format;
            }
        }

        vkCmdBeginRendering(impl->buffer, &info);
    }

    void CommandList::EndRendering() const
    {
        vkCmdEndRendering(impl->buffer);
    }

    void CommandList::Draw(u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance) const
    {
        vkCmdDraw(impl->buffer, vertices, instances, firstVertex, firstInstance);
    }

    void CommandList::DrawIndexed(u32 indices, u32 instances, u32 firstIndex, u32 vertexOffset, u32 firstInstance) const
    {
        vkCmdDrawIndexed(impl->buffer, indices, instances, firstIndex, vertexOffset, firstInstance);
    }

    void CommandList::BindIndexBuffer(HBuffer indexBuffer, IndexType indexType, u64 offset) const
    {
        vkCmdBindIndexBuffer(impl->buffer, indexBuffer->buffer, offset, GetVulkanIndexType(indexType));
    }

    NOVA_NO_INLINE
    void CommandList::SetScissors(Span<Rect2I> scissors) const
    {
        auto vkScissors = NOVA_ALLOC_STACK(VkRect2D, scissors.size());

        for (u32 i = 0; i < scissors.size(); ++i) {
            vkScissors[i] = {
                {    scissors[i].offset.x,      scissors[i].offset.y },
                {u32(scissors[i].extent.x), u32(scissors[i].extent.y)},
            };
        }

        vkCmdSetScissorWithCount(impl->buffer, u32(scissors.size()), vkScissors);
    }

    void CommandList::ClearColor(u32 attachment, Vec4 color, Vec2U size, Vec2I offset) const
    {
        vkCmdClearAttachments(
            impl->buffer, 1, nova::Temp(VkClearAttachment {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .colorAttachment = attachment,
                .clearValue = {{{ color.r, color.g, color.b, color.a }}},
            }),
            1, nova::Temp(VkClearRect {
                .rect = { { offset.x, offset.y }, { size.x, size.y } },
                .baseArrayLayer = 0,
                .layerCount = 1,
            }));
    }

    void CommandList::ClearDepth(f32 depth, Vec2U size, Vec2I offset) const
    {
        vkCmdClearAttachments(
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
        vkCmdClearAttachments(
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
}