#include "nova_VulkanRHI.hpp"

namespace nova
{
    void VulkanContext::Cmd_BeginRendering(CommandList cmd, Span<Texture> colorAttachments, Texture depthAttachment, Texture stencilAttachment)
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

        auto& attach0 = Get(colorAttachments[0]);
        state.renderingExtent = Vec2U(attach0.extent);

        VkRenderingInfo info {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = { {}, { attach0.extent.x, attach0.extent.y } },
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
}