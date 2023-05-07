#include <VulkanBackend/VulkanBackend.hpp>

#include <Renderer/Renderer.hpp>

int main()
{
    auto ctx = pyr::CreateContext(true);
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1080, "test", nullptr, nullptr);

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(ctx.instance, window, nullptr, &surface);

    auto swapchain = ctx.CreateSwapchain(surface,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT
        | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        | VK_IMAGE_USAGE_STORAGE_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_PRESENT_MODE_MAILBOX_KHR);

    pyr::Renderer renderer;
    renderer.Init(&ctx);

//     auto vertShader = ctx.CreateShader(
//         VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT,
//         "vertex",
//         R"(
// #version 460

// const vec2 positions[3] = vec2[] (
//     vec2(-1, 1),
//     vec2(1, 1),
//     vec2(0, -1)
// );

// const vec3 colors[3] = vec3[] (
//     vec3(1, 0, 0),
//     vec3(0, 1, 0),
//     vec3(0, 0, 1)
// );

// layout(location = 0) out vec3 color;

// void main()
// {
//     color = colors[gl_VertexIndex];
//     gl_Position = vec4(positions[gl_VertexIndex] * vec2(0.75), 0, 1);
// }
//         )");

//     auto fragShader = ctx.CreateShader(
//         VK_SHADER_STAGE_FRAGMENT_BIT, 0,
//         "fragment",
//         R"(
// #version 460

// layout(location = 0) in vec3 inColor;

// layout(location = 0) out vec4 fragColor;

// void main()
// {
//     fragColor = vec4(inColor, inColor.r);
// }
//         )");

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ctx.GetNextImage(swapchain);

        renderer.Draw(*swapchain.image);

        // ctx.Transition(ctx.cmd, *swapchain.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        // vkCmdClearColorImage(ctx.cmd, swapchain.image->image,
        //     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        //     pyr::Temp(VkClearColorValue {{ 1.f, 0.f, 0.f, 1.f }}),
        //     1, pyr::Temp(VkImageSubresourceRange { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }));

        // ctx.Transition(ctx.cmd, *swapchain.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        // vkCmdBeginRendering(ctx.cmd, pyr::Temp(VkRenderingInfo {
        //     .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        //     .renderArea = { {}, swapchain.extent },
        //     .layerCount = 1,
        //     .colorAttachmentCount = 1,
        //     .pColorAttachments = pyr::Temp(VkRenderingAttachmentInfo {
        //         .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        //         .imageView = swapchain.image->view,
        //         .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        //         .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        //         .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        //         .clearValue = {{{ 0.f, 0.f, 0.f, 1.f }}},
        //     }),
        // }));

        // vkCmdClearAttachments(ctx.cmd,
        //     1, pyr::Temp(VkClearAttachment {
        //         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        //         .colorAttachment = 0,
        //         .clearValue = {{{ 0.f, 0.f, 1.f, 1.f }}},
        //     }),
        //     1, pyr::Temp(VkClearRect {
        //         .rect = { {}, swapchain.extent },
        //         .baseArrayLayer = 0,
        //         .layerCount = 1,
        //     }));

        // bool blend = true;
        // bool depth = false;
        // bool cull = false;
        // vkCmdSetPrimitiveTopologyEXT(ctx.cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        // if (depth)
        // {
        //     vkCmdSetDepthTestEnableEXT(ctx.cmd, VK_TRUE);
        //     vkCmdSetDepthWriteEnableEXT(ctx.cmd, VK_TRUE);
        //     vkCmdSetDepthCompareOpEXT(ctx.cmd, VK_COMPARE_OP_GREATER);
        // }
        // if (cull)
        // {
        //     vkCmdSetCullModeEXT(ctx.cmd, VK_CULL_MODE_BACK_BIT);
        //     vkCmdSetFrontFaceEXT(ctx.cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        // }
        // vkCmdSetColorWriteMaskEXT(ctx.cmd, 0, 1, pyr::Temp<VkColorComponentFlags>(
        //     VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        //     | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        // ));
        // if (blend)
        // {
        //     vkCmdSetColorBlendEnableEXT(ctx.cmd, 0, 1, pyr::Temp(VK_TRUE));
        //     vkCmdSetColorBlendEquationEXT(ctx.cmd, 0, 1, pyr::Temp(VkColorBlendEquationEXT {
        //         .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        //         .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        //         .colorBlendOp = VK_BLEND_OP_ADD,
        //         .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        //         .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        //         .alphaBlendOp = VK_BLEND_OP_ADD,
        //     }));
        // }
        // else
        // {
        //     vkCmdSetColorBlendEnableEXT(ctx.cmd, 0, 1, pyr::Temp(VK_FALSE));
        // }

        // pyr::vkCmdBindShadersEXT(ctx.cmd, 2,
        //     std::array { vertShader.stage, fragShader.stage }.data(),
        //     std::array { vertShader.shader, fragShader.shader }.data());

        // vkCmdSetViewportWithCount(ctx.cmd, 1, pyr::Temp(VkViewport {
        //     0.f, float(swapchain.extent.height),
        //     float(swapchain.extent.width), -float(swapchain.extent.height),
        //     0.f, 1.f
        // }));
        // vkCmdSetScissorWithCount(ctx.cmd, 1, pyr::Temp(VkRect2D { { 0, 0 }, swapchain.extent }));
        // vkCmdDraw(ctx.cmd, 3, 1, 0, 0);
        // vkCmdEndRendering(ctx.cmd);

        // ctx.Flush();

        ctx.Present(swapchain);
    }

    vkDeviceWaitIdle(ctx.device);

    glfwDestroyWindow(window);
    glfwTerminate();
}