#include "nova_ImGui.hpp"

#include <nova/rhi/nova_RHI_Impl.hpp>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

namespace nova
{
    NOVA_DEFINE_HANDLE_OPERATIONS(ImGuiWrapper)

    ImGuiWrapper::ImGuiWrapper(Context context,
            CommandList cmd, Format format, GLFWwindow* window,
            const ImGuiConfig& config)
        : ImplHandle(new ImGuiWrapperImpl)
    {
        impl->context = context;

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
        }), context->pAlloc, &impl->renderPass));

        // Create ImGui context and initialize

        impl->imguiCtx = ImGui::CreateContext();
        impl->lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(impl->imguiCtx);

        auto& io = ImGui::GetIO();
        io.ConfigFlags |= config.flags;
        ImGui_ImplGlfw_InitForVulkan(window, true);
        ImGui_ImplVulkan_Init(Temp(ImGui_ImplVulkan_InitInfo {
            .Instance = context->instance,
            .PhysicalDevice = context->gpu,
            .Device = context->device,
            .QueueFamily = context->graphics->family,
            .Queue = context->graphics->handle,
            .DescriptorPool = impl->context->descriptorPool,
            .Subpass = 0,
            .MinImageCount = framesInFlight,
            .ImageCount = framesInFlight,
            .CheckVkResultFn = [](VkResult r) { VkCall(r); },
        }), impl->renderPass);

        // Rescale UI and fonts

        ImGui::GetStyle().ScaleAllSizes(config.uiScale);
        auto fontConfig = ImFontConfig();
        fontConfig.GlyphOffset = ImVec2(config.glyphOffset.x, config.glyphOffset.y);
        ImGui::GetIO().Fonts->ClearFonts();
        ImGui::GetIO().Fonts->AddFontFromFileTTF(config.font, config.fontSize, &fontConfig);
        ImGui_ImplVulkan_CreateFontsTexture(cmd->buffer);

        ImGui::SetCurrentContext(impl->lastImguiCtx);
    }

    ImGuiWrapperImpl::~ImGuiWrapperImpl()
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

    void ImGuiWrapper::BeginFrame() const
    {
        impl->lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(impl->imguiCtx);

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
        impl->ended = false;
    }

    void ImGuiWrapper::EndFrame() const
    {
        if (!impl->ended)
        {
            ImGui::Render();
            impl->ended = true;

            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                auto contextBackup = glfwGetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(contextBackup);
            }

            ImGui::SetCurrentContext(impl->lastImguiCtx);
        }
    }

    bool ImGuiWrapper::HasDrawData() const
    {
        return impl->ended;
    }

    void ImGuiWrapper::DrawFrame(CommandList cmd, Texture texture) const
    {
        EndFrame();

        impl->lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(impl->imguiCtx);

        auto context = impl->context;

        if (Vec2U(texture.GetExtent()) != impl->lastSize
            || texture->usage != impl->lastUsage)
        {
            impl->lastSize = Vec2U(texture.GetExtent());
            impl->lastUsage = texture->usage;

            vkDestroyFramebuffer(context->device, impl->framebuffer, context->pAlloc);

            VkCall(vkCreateFramebuffer(context->device, Temp(VkFramebufferCreateInfo {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext = Temp(VkFramebufferAttachmentsCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,
                    .attachmentImageInfoCount = 1,
                    .pAttachmentImageInfos = Temp(VkFramebufferAttachmentImageInfo {
                        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
                        .usage = texture->usage,
                        .width = impl->lastSize.x,
                        .height = impl->lastSize.y,
                        .layerCount = 1,
                        .viewFormatCount = 1,
                        .pViewFormats = &texture->format,
                    }),
                }),
                .flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,
                .renderPass = impl->renderPass,
                .attachmentCount = 1,
                .width = impl->lastSize.x,
                .height = impl->lastSize.y,
                .layers = 1,
            }), context->pAlloc, &impl->framebuffer));
        }

        cmd.Transition(texture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);

        vkCmdBeginRenderPass(cmd->buffer, Temp(VkRenderPassBeginInfo {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = Temp(VkRenderPassAttachmentBeginInfo {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,
                .attachmentCount = 1,
                .pAttachments = &texture->view,
            }),
            .renderPass = impl->renderPass,
            .framebuffer = impl->framebuffer,
            .renderArea = { {}, { impl->lastSize.x, impl->lastSize.y } },
        }), VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd->buffer);
        vkCmdEndRenderPass(cmd->buffer);

        ImGui::SetCurrentContext(impl->lastImguiCtx);
    }
}
