#pragma once

#include <nova/rhi/nova_RHI.hpp>

namespace nova
{
    struct ImGuiWrapperImpl : ImplBase
    {
        ContextImpl* context = {};
        VkRenderPass renderPass = {};
        std::vector<VkFramebuffer> framebuffers;
        VkDescriptorPool descriptorPool = {};
        VkSwapchainKHR lastSwapchain = {};

        ImGuiContext* imguiCtx = {};
        ImGuiContext* lastImguiCtx = {};

    public:
        ~ImGuiWrapperImpl();
    };

    struct ImGuiWrapper : ImplHandle<ImGuiWrapperImpl>
    {
    public:
        ImGuiWrapper() = default;
        ImGuiWrapper(Context context,
            CommandList cmd, Swapchain swapchain, GLFWwindow* window,
            i32 imguiFlags, u32 framesInFlight = 2);

        void BeginFrame() const;
        void EndFrame(CommandList cmd, Swapchain swapchain) const;
    };
}