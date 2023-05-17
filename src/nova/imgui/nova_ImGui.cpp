#include "nova_ImGui.hpp"

namespace nova
{
    ImGuiWrapper* ImGuiWrapper::Create(Context* context, Swapchain* swapchain, GLFWwindow* window, int imguiFlags)
    {
        auto imgui = new ImGuiWrapper;
        imgui->context = context;

        VkCall(vkCreateRenderPass(context->device, Temp(VkRenderPassCreateInfo {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = Temp(VkAttachmentDescription {
                .format = swapchain->format.format,
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
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            }),
        }), nullptr, &imgui->renderPass));

        // Create fixed sampler only descriptor pool for ImGui
        // All application descriptor sets will be managed by descriptor buffers

        constexpr u32 MaxSamplers = 65'536;
        VkCall(vkCreateDescriptorPool(context->device, Temp(VkDescriptorPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = MaxSamplers,
            .poolSizeCount = 1,
            .pPoolSizes = Temp(VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MaxSamplers }),
        }), nullptr, &imgui->descriptorPool));

        // Create ImGui context and initialize

        imgui->imguiCtx = ImGui::CreateContext();
        imgui->lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui->imguiCtx);

        auto& io = ImGui::GetIO();
        io.ConfigFlags |= imguiFlags;
        ImGui_ImplGlfw_InitForVulkan(window, true);
        ImGui_ImplVulkan_Init(Temp(ImGui_ImplVulkan_InitInfo {
            .Instance = context->instance,
            .PhysicalDevice = context->gpu,
            .Device = context->device,
            .QueueFamily = context->graphics->family,
            .Queue = context->graphics->handle,
            .DescriptorPool = imgui->descriptorPool,
            .Subpass = 0,
            .MinImageCount = 2,
            .ImageCount = 2,
            .CheckVkResultFn = [](VkResult r) { VkCall(r); },
        }), imgui->renderPass);

        // Rescale UI and fonts
        // TODO: Don't hardcode 150%

        ImGui::GetStyle().ScaleAllSizes(1.5f);
        auto fontConfig = ImFontConfig();
        fontConfig.GlyphOffset = ImVec2(1.f, 1.67f);
        ImGui::GetIO().Fonts->ClearFonts();
        ImGui::GetIO().Fonts->AddFontFromFileTTF("assets/fonts/CONSOLA.TTF", 20, &fontConfig);
        ImGui_ImplVulkan_CreateFontsTexture(context->transferCmd->buffer);
        context->graphics->Submit(context->transferCmd, nullptr, context->transferFence);
        context->transferFence->Wait();
        context->transferCommandPool->Reset();
        context->transferCmd = context->transferCommandPool->Begin();

        ImGui::SetCurrentContext(imgui->lastImguiCtx);

        return imgui;
    }

    void ImGuiWrapper::Destroy(ImGuiWrapper* imgui)
    {
        auto context = imgui->context;

        for (auto framebuffer : imgui->framebuffers)
            vkDestroyFramebuffer(context->device, framebuffer, context->pAlloc);
        vkDestroyRenderPass(context->device, imgui->renderPass, context->pAlloc);
        vkDestroyDescriptorPool(context->device, imgui->descriptorPool, context->pAlloc);

        imgui->lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui->imguiCtx);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::SetCurrentContext(imgui->lastImguiCtx);
        ImGui::DestroyContext(imgui->imguiCtx);

        delete imgui;
    }

    void ImGuiWrapper::BeginFrame()
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
    }

    void ImGuiWrapper::EndFrame(CommandList* cmd, Swapchain& swapchain)
    {
        ImGui::Render();

        if (lastSwapchain != swapchain.swapchain)
        {
            lastSwapchain = swapchain.swapchain;

            framebuffers.resize(swapchain.images.size());
            for (u32 i = 0; i < swapchain.images.size(); ++i)
            {
                vkDestroyFramebuffer(context->device, framebuffers[i], nullptr);
                VkCall(vkCreateFramebuffer(context->device, Temp(VkFramebufferCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                    .renderPass = renderPass,
                    .attachmentCount = 1,
                    .pAttachments = &swapchain.images[i]->view,
                    .width = swapchain.extent.width,
                    .height = swapchain.extent.height,
                    .layers = 1,
                }), nullptr, &framebuffers[i]));
            }
        }

        cmd->Transition(swapchain.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vkCmdBeginRenderPass(cmd->buffer, Temp(VkRenderPassBeginInfo {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderPass,
            .framebuffer = framebuffers[swapchain.index],
            .renderArea = { {}, swapchain.extent },
        }), VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd->buffer);
        vkCmdEndRenderPass(cmd->buffer);

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