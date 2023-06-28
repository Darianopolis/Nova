#include "nova_VulkanRHI.hpp"

namespace nova
{
    void VulkanContext::Cmd_BeginRendering(CommandList cmd, Rect2D region, Span<Texture> colorAttachments, Texture depthAttachment, Texture stencilAttachment)
    {
        auto& state = Get(Get(cmd).state);

        state.colorAttachmentsFormats.resize(colorAttachments.size());

        auto colorAttachmentInfos = NOVA_ALLOC_STACK(VkRenderingAttachmentInfo, colorAttachments.size());
        for (u32 i = 0; i < colorAttachments.size(); ++i)
        {
            auto& texture = Get(colorAttachments[i]);

            Cmd_Transition(cmd, colorAttachments[i], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);

            colorAttachmentInfos[i] = VkRenderingAttachmentInfo {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = texture.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };

            state.colorAttachmentsFormats[i] = Texture_GetFormat(colorAttachments[i]);
        }

        state.renderingExtent = region.extent;

        VkRenderingInfo info {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = { { region.offset.x, region.offset.y }, { region.extent.x, region.extent.y } },
            .layerCount = 1,
            .colorAttachmentCount = u32(colorAttachments.size()),
            .pColorAttachments = colorAttachmentInfos,
        };

        VkRenderingAttachmentInfo depthInfo = {};
        VkRenderingAttachmentInfo stencilInfo = {};

        if (depthAttachment == stencilAttachment)
        {
            if (textures.IsValid(depthAttachment))
            {
                Cmd_Transition(cmd, depthAttachment,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR);

                depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depthInfo.imageView = Get(depthAttachment).view;
                depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                info.pDepthAttachment = &depthInfo;
                info.pStencilAttachment = &depthInfo;

                state.depthAttachmentFormat = Texture_GetFormat(depthAttachment);
                state.stencilAttachmentFormat = Texture_GetFormat(stencilAttachment);
            }
        }
        else
        {
            if (textures.IsValid(depthAttachment))
            {
                Cmd_Transition(cmd, depthAttachment,
                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR);

                depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depthInfo.imageView = Get(depthAttachment).view;
                depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                info.pDepthAttachment = &depthInfo;

                state.depthAttachmentFormat = Texture_GetFormat(depthAttachment);
            }

            if (textures.IsValid(stencilAttachment))
            {
                Cmd_Transition(cmd, stencilAttachment,
                    VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR);

                stencilInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                stencilInfo.imageView = Get(stencilAttachment).view;
                stencilInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                info.pStencilAttachment = &stencilInfo;

                state.stencilAttachmentFormat = Texture_GetFormat(stencilAttachment);
            }
        }

        vkCmdBeginRendering(Get(cmd).buffer, &info);
    }

    void VulkanContext::Cmd_EndRendering(CommandList id)
    {
        vkCmdEndRendering(Get(id).buffer);
    }

    void VulkanContext::Cmd_Draw(CommandList cmd, u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance)
    {
        vkCmdDraw(Get(cmd).buffer, vertices, instances, firstVertex, firstInstance);
    }

    void VulkanContext::Cmd_DrawIndexed(CommandList cmd, u32 indices, u32 instances, u32 firstIndex, u32 vertexOffset, u32 firstInstance)
    {
        vkCmdDrawIndexed(Get(cmd).buffer, indices, instances, firstIndex, vertexOffset, firstInstance);
    }

    void VulkanContext::Cmd_BindIndexBuffer(CommandList cmd, Buffer buffer, IndexType indexType, u64 offset)
    {
        vkCmdBindIndexBuffer(Get(cmd).buffer, Get(buffer).buffer, offset, VkIndexType(indexType));
    }

    void VulkanContext::Cmd_ClearColor(CommandList cmd, u32 attachment, Vec4 color, Vec2U size, Vec2I offset)
    {
        vkCmdClearAttachments(
            Get(cmd).buffer, 1, nova::Temp(VkClearAttachment {
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

    void VulkanContext::Cmd_ClearDepth(CommandList cmd, f32 depth, Vec2U size, Vec2I offset)
    {
        vkCmdClearAttachments(
            Get(cmd).buffer, 1, nova::Temp(VkClearAttachment {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .clearValue = { .depthStencil = { .depth = depth } },
            }),
            1, nova::Temp(VkClearRect {
                .rect = { { offset.x, offset.y }, { size.x, size.y } },
                .baseArrayLayer = 0,
                .layerCount = 1,
            }));
    }

    void VulkanContext::Cmd_ClearStencil(CommandList cmd, u32 value, Vec2U size, Vec2I offset)
    {
        vkCmdClearAttachments(
            Get(cmd).buffer, 1, nova::Temp(VkClearAttachment {
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