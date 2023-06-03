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
        ImGuiWrapper() = default;
        ImGuiWrapper(Context& context,
            Ref<CommandList> cmd, Swapchain& swapchain, GLFWwindow* window,
            i32 imguiFlags, u32 framesInFlight = 2);
        ~ImGuiWrapper();

        NOVA_DEFAULT_MOVE_DECL(ImGuiWrapper)

        void BeginFrame();
        void EndFrame(Ref<CommandList> cmd, Swapchain& swapchain);
    };
}