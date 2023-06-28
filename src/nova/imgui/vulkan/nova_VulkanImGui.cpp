#include "nova_VulkanImGui.hpp"

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

namespace nova
{
    VulkanImGuiWrapper::VulkanImGuiWrapper(Context* _context,
            CommandList cmd, Format format, GLFWwindow* window,
            const ImGuiConfig& config)
        : context(&dynamic_cast<VulkanContext&>(*_context))
    {
        u32 framesInFlight = std::max(config.imageCount, 2u);

        VkCall(vkCreateRenderPass(context->device, Temp(VkRenderPassCreateInfo {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = Temp(VkAttachmentDescription {
                .format = VkFormat(format),
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }),
            .subpassCount = 1,
            .pSubpasses = Temp(VkSubpassDescription {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments = Temp(VkAttachmentReference {
                    .attachment = 0,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                }),
            }),
            .dependencyCount = 1,
            .pDependencies = Temp(VkSubpassDependency {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
            }),
        }), context->pAlloc, &renderPass));

        // Create ImGui context and initialize

        imguiCtx = ImGui::CreateContext();
        lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiCtx);

        auto& io = ImGui::GetIO();
        io.ConfigFlags |= config.flags;
        ImGui_ImplGlfw_InitForVulkan(window, true);
        ImGui_ImplVulkan_Init(Temp(ImGui_ImplVulkan_InitInfo {
            .Instance = context->instance,
            .PhysicalDevice = context->gpu,
            .Device = context->device,
            .QueueFamily = context->Get(context->graphicQueues.front()).family,
            .Queue = context->Get(context->graphicQueues.front()).handle,
            .DescriptorPool = context->descriptorPool,
            .Subpass = 0,
            .MinImageCount = framesInFlight,
            .ImageCount = framesInFlight,
            .CheckVkResultFn = [](VkResult r) { VkCall(r); },
        }), renderPass);

        // Rescale UI and fonts

        ImGui::GetStyle().ScaleAllSizes(config.uiScale);
        auto fontConfig = ImFontConfig();
        fontConfig.GlyphOffset = ImVec2(config.glyphOffset.x, config.glyphOffset.y);
        ImGui::GetIO().Fonts->ClearFonts();
        ImGui::GetIO().Fonts->AddFontFromFileTTF(config.font, config.fontSize, &fontConfig);
        ImGui_ImplVulkan_CreateFontsTexture(context->Get(cmd).buffer);

        ImGui::SetCurrentContext(lastImguiCtx);
    }

    VulkanImGuiWrapper::~VulkanImGuiWrapper()
    {
        if (!renderPass)
            return;

        vkDestroyFramebuffer(context->device, framebuffer, context->pAlloc);
        vkDestroyRenderPass(context->device, renderPass, context->pAlloc);

        lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiCtx);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::SetCurrentContext(lastImguiCtx);
        ImGui::DestroyContext(imguiCtx);
    }

    void VulkanImGuiWrapper::BeginFrame()
    {
        lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiCtx);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiDockNodeFlags dockspaceFlags = 0;
            ImGuiWindowFlags windowFlags = 0;

            // TODO: More configuration

            // Make dockspace fullscreen
            auto viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
            windowFlags |= ImGuiWindowFlags_NoTitleBar
                | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoBringToFrontOnFocus
                | ImGuiWindowFlags_NoNavFocus;

            // Pass through background
            windowFlags |= ImGuiWindowFlags_NoBackground;
            dockspaceFlags |= ImGuiDockNodeFlags_PassthruCentralNode;

            // Register dockspace
            ImGui::Begin("Dockspace", Temp(true), windowFlags);
            ImGui::PopStyleVar(3);
            ImGui::DockSpace(ImGui::GetID("DockspaceID"), ImVec2(0.f, 0.f), dockspaceFlags);
            ImGui::End();
        }
        ended = false;
    }

    void VulkanImGuiWrapper::EndFrame()
    {
        if (!ended)
        {
            ImGui::Render();
            ended = true;

            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                auto contextBackup = glfwGetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(contextBackup);
            }

            ImGui::SetCurrentContext(lastImguiCtx);
        }
    }

    bool VulkanImGuiWrapper::HasDrawData()
    {
        return ended;
    }

    void VulkanImGuiWrapper::DrawFrame(CommandList cmd, Texture texture)
    {
        EndFrame();

        lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiCtx);

        if (Vec2U(context->Texture_GetExtent(texture)) != lastSize
            || context->Get(texture).usage != lastUsage)
        {
            lastSize = Vec2U(context->Texture_GetExtent(texture));
            lastUsage = context->Get(texture).usage;

            vkDestroyFramebuffer(context->device, framebuffer, context->pAlloc);

            VkCall(vkCreateFramebuffer(context->device, Temp(VkFramebufferCreateInfo {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext = Temp(VkFramebufferAttachmentsCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,
                    .attachmentImageInfoCount = 1,
                    .pAttachmentImageInfos = Temp(VkFramebufferAttachmentImageInfo {
                        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
                        .usage = context->Get(texture).usage,
                        .width = lastSize.x,
                        .height = lastSize.y,
                        .layerCount = 1,
                        .viewFormatCount = 1,
                        .pViewFormats = &context->Get(texture).format,
                    }),
                }),
                .flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,
                .renderPass = renderPass,
                .attachmentCount = 1,
                .width = lastSize.x,
                .height = lastSize.y,
                .layers = 1,
            }), context->pAlloc, &framebuffer));
        }

        context->Cmd_Transition(cmd, texture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);

        vkCmdBeginRenderPass(context->Get(cmd).buffer, Temp(VkRenderPassBeginInfo {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = Temp(VkRenderPassAttachmentBeginInfo {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,
                .attachmentCount = 1,
                .pAttachments = &context->Get(texture).view,
            }),
            .renderPass = renderPass,
            .framebuffer = framebuffer,
            .renderArea = { {}, { lastSize.x, lastSize.y } },
        }), VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), context->Get(cmd).buffer);
        vkCmdEndRenderPass(context->Get(cmd).buffer);

        ImGui::SetCurrentContext(lastImguiCtx);
    }
}
