#include <nova/ui/nova_ImGui.hpp>

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

#include <ImGuizmo.h>

namespace nova
{
    ImGuiLayer::ImGuiLayer(Context _context,
            CommandList cmd, Format format, GLFWwindow* window,
            const ImGuiConfig& config)
        : context(_context)
    {
        u32 framesInFlight = std::max(config.imageCount, 2u);

        // Create ImGui context and initialize

        imguiCtx = ImGui::CreateContext();
        lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiCtx);

        constexpr u32 SampledImageDescriptors = 65'536;
        VkCall(vkCreateDescriptorPool(context->device, Temp(VkDescriptorPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
                | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets = SampledImageDescriptors,
            .poolSizeCount = 1,
            .pPoolSizes = std::array {
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLER, SampledImageDescriptors },
            }.data(),
        }), context->pAlloc, (VkDescriptorPool*)&descriptorPool));

        auto& io = ImGui::GetIO();
        io.ConfigFlags |= config.flags;
        ImGui_ImplGlfw_InitForVulkan(window, true);
        ImGui_ImplVulkan_Init(Temp(ImGui_ImplVulkan_InitInfo {
            .Instance = context->instance,
            .PhysicalDevice = context->gpu,
            .Device = context->device,
            .QueueFamily = context->graphicQueues.front()->family,
            .Queue = context->graphicQueues.front()->handle,
            .DescriptorPool = (VkDescriptorPool)descriptorPool,
            .Subpass = 0,
            .MinImageCount = framesInFlight,
            .ImageCount = framesInFlight,
            .UseDynamicRendering = true,
            .ColorAttachmentFormat = GetVulkanFormat(format),
            .CheckVkResultFn = [](VkResult r) { VkCall(r); },
        }), nullptr);

        // Rescale UI and fonts

        ImGui::GetStyle().ScaleAllSizes(config.uiScale);
        auto fontConfig = ImFontConfig();
        fontConfig.GlyphOffset = ImVec2(config.glyphOffset.x, config.glyphOffset.y);
        ImGui::GetIO().Fonts->ClearFonts();
        ImGui::GetIO().Fonts->AddFontFromFileTTF(config.font, config.fontSize, &fontConfig);
        ImGui_ImplVulkan_CreateFontsTexture(cmd->buffer);

        ImGui::SetCurrentContext(lastImguiCtx);
    }

    ImGuiLayer::~ImGuiLayer()
    {
        lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiCtx);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::SetCurrentContext(lastImguiCtx);
        ImGui::DestroyContext(imguiCtx);
        vkDestroyDescriptorPool(context->device, (VkDescriptorPool)descriptorPool, context->pAlloc);
    }

    void ImGuiLayer::BeginFrame_(DockspaceWindowFn fn, void* payload)
    {
        lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiCtx);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::BeginFrame();

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiDockNodeFlags dockspaceFlags = 0;
            ImGuiWindowFlags dockspaceWindowFlags = 0;

            // Make dockspace fullscreen
            auto viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
            dockspaceWindowFlags |= ImGuiWindowFlags_NoTitleBar
                | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoBringToFrontOnFocus
                | ImGuiWindowFlags_NoNavFocus;

            if (dockMenuBar) {
                dockspaceWindowFlags |= ImGuiWindowFlags_MenuBar;
            }

            if (noDockBg) {
                // Pass through background
                dockspaceWindowFlags |= ImGuiWindowFlags_NoBackground;
                dockspaceFlags |= ImGuiDockNodeFlags_PassthruCentralNode;
            }

            // Register dockspace
            ImGui::Begin("Dockspace", Temp(true), dockspaceWindowFlags);
            ImGui::PopStyleVar(3);
            ImGui::DockSpace(ImGui::GetID("DockspaceID"), ImVec2(0.f, 0.f), dockspaceFlags);

            fn(payload, *this);

            ImGui::End();

        }
        ended = false;
    }

    void ImGuiLayer::EndFrame()
    {
        if (!ended) {
            ImGui::Render();
            ended = true;

            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                auto contextBackup = glfwGetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(contextBackup);
            }

            ImGui::SetCurrentContext(lastImguiCtx);
        }
    }

    bool ImGuiLayer::HasDrawData()
    {
        return ended;
    }

    void ImGuiLayer::DrawFrame(CommandList cmd, Texture texture)
    {
        EndFrame();

        lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiCtx);

        cmd.Transition(texture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);

        vkCmdBeginRendering(cmd->buffer, Temp(VkRenderingInfo {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = { {}, { texture->extent.x, texture->extent.y } },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = Temp(VkRenderingAttachmentInfo {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = texture->view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }),
        }));
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd->buffer);

        vkCmdEndRendering(cmd->buffer);

        ImGui::SetCurrentContext(lastImguiCtx);
    }
}
