#pragma once

#include <nova/rhi/nova_RHI.hpp>

namespace nova
{
    NOVA_DECLARE_STRUCTURE(ImGuiWrapper)

    struct ImGuiWrapper : RefCounted
    {
        ImGuiContext* imguiCtx = {};
        ImGuiContext* lastImguiCtx = {};

        ContextRef ctx = {};
        VkRenderPass renderPass = {};
        std::vector<VkFramebuffer> framebuffers;
        VkDescriptorPool descriptorPool = {};
        VkSwapchainKHR lastSwapchain = {};
    public:
        static ImGuiWrapperRef Create(Context& ctx, Swapchain& swapchain, GLFWwindow* window, int imguiFlags);
        ~ImGuiWrapper();

        void BeginFrame();
        void EndFrame(Swapchain& swapchain);
    };
}