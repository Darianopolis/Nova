#pragma once

#include "Pyrite_Core.hpp"

namespace pyr
{
    struct ImGuiBackend
    {
        ImGuiContext* imguiCtx = {};
        ImGuiContext* lastImguiCtx = {};

        nova::ContextRef ctx = {};
        VkRenderPass renderPass = {};
        std::vector<VkFramebuffer> framebuffers;
        VkDescriptorPool descriptorPool = {};
        VkSwapchainKHR lastSwapchain = {};
    public:
        void Init(nova::Context& ctx, nova::Swapchain& swapchain, GLFWwindow* window, int imguiFlags);
        void BeginFrame();
        void EndFrame(nova::Swapchain& swapchain);
    };
}