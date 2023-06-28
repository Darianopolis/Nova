#pragma once

#include <nova/imgui/nova_ImGui.hpp>

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

namespace nova
{
    struct VulkanImGuiWrapper : ImGuiWrapper
    {
        VulkanContext*          context = {};
        VkRenderPass   renderPass = {};
        VkFramebuffer framebuffer = {};

        Vec2U              lastSize = {};
        VkImageUsageFlags lastUsage = {};

        ImGuiContext*     imguiCtx = {};
        ImGuiContext* lastImguiCtx = {};

        bool ended = false;

    public:
        VulkanImGuiWrapper(Context* context,
            CommandList cmd, Format format, GLFWwindow* window,
            const ImGuiConfig& config);

        ~VulkanImGuiWrapper() final;

        void BeginFrame() final;
        void EndFrame() final;

        bool HasDrawData() final;
        void DrawFrame(CommandList cmd, Texture texture) final;
    };
}