#pragma once

#include <VulkanBackend/VulkanBackend.hpp>

namespace pyr
{
    struct ImGuiBackend
    {
        ImGuiContext* imguiCtx = {};
        ImGuiContext* lastImguiCtx = {};

        Context* ctx = {};
        VkRenderPass renderPass = {};
        std::vector<VkFramebuffer> framebuffers;
        VkDescriptorPool descriptorPool = {};
        VkSwapchainKHR lastSwapchain = {};
    public:
        void Init(Context& ctx, Swapchain& swapchain, GLFWwindow* window, int imguiFlags);
        void BeginFrame();
        void EndFrame(Swapchain& swapchain);
    };
}