#pragma once

#include <nova/rhi/nova_RHI.hpp>

namespace nova
{
    struct ImGuiWrapper
    {
        ImGuiContext* imguiCtx = {};
        ImGuiContext* lastImguiCtx = {};

        Context* context = {};
        VkRenderPass renderPass = {};
        std::vector<VkFramebuffer> framebuffers;
        VkDescriptorPool descriptorPool = {};
        VkSwapchainKHR lastSwapchain = {};
    public:
        static ImGuiWrapper* Create(Context* context, Swapchain* swapchain, GLFWwindow* window, int imguiFlags);
        static void Destroy(ImGuiWrapper* imgui);

        void BeginFrame();
        void EndFrame(VkCommandBuffer cmd, Swapchain& swapchain);
    };
}