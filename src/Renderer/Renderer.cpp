#include "Renderer.hpp"

namespace pyr
{
    void Renderer::Init(Context* _ctx)
    {
        ctx = _ctx;

        vertShader = ctx->CreateShader(
            VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT,
            "assets/shaders/triangle.vert");

        fragShader = ctx->CreateShader(
            VK_SHADER_STAGE_FRAGMENT_BIT, 0,
            "assets/shaders/triangle.frag");
    }

    void Renderer::Draw(Image& target)
    {
        auto cmd = ctx->cmd;

        // Begin rendering

        ctx->Transition(cmd, target, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vkCmdBeginRendering(cmd, Temp(VkRenderingInfo {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = { {}, { target.extent.x, target.extent.y } },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = Temp(VkRenderingAttachmentInfo {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = target.view,
                .imageLayout = target.layout,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {{{ 0.1f, 0.1f, 0.1f, 1.f }}},
            }),
        }));

        vkCmdSetScissorWithCount(cmd, 1, Temp(VkRect2D { {}, { target.extent.x, target.extent.y } }));
        vkCmdSetViewportWithCount(cmd, 1, Temp(VkViewport {
            0.f, f32(target.extent.y),
            f32(target.extent.x), -f32(target.extent.y),
            0.f, 1.f
        }));

        // Set blend, depth, cull dynamic state

        bool blend = true;
        bool depth = false;
        bool cull = false;

        if (depth)
        {
            vkCmdSetDepthTestEnable(cmd, true);
            vkCmdSetDepthWriteEnable(cmd, true);
            vkCmdSetDepthCompareOp(cmd, VK_COMPARE_OP_GREATER);
        }

        if (cull)
        {
            vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);
            vkCmdSetFrontFace(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        }

        vkCmdSetColorWriteMaskEXT(cmd, 0, 1, Temp<VkColorComponentFlags>(
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        ));
        if (blend)
        {
            vkCmdSetColorBlendEnableEXT(cmd, 0, 1, Temp<VkBool32>(true));
            vkCmdSetColorBlendEquationEXT(cmd, 0, 1, Temp(VkColorBlendEquationEXT {
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .alphaBlendOp = VK_BLEND_OP_ADD,
            }));
        }
        else
        {
            vkCmdSetColorBlendEnableEXT(cmd, 0, 1, Temp<VkBool32>(false));
        }

        // Bind shaders and draw

        vkCmdSetPrimitiveTopology(cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        vkCmdBindShadersEXT(cmd, 2,
            std::array { vertShader.stage, fragShader.stage }.data(),
            std::array { vertShader.shader, fragShader.shader }.data());
        vkCmdDraw(cmd, 3, 1, 0, 0);

        // End rendering

        vkCmdEndRendering(cmd);
    }
}