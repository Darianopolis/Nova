#pragma once

#include <nova/rhi/nova_RHI.hpp>

namespace nova
{
    struct ImGuiWrapperImpl : ImplBase
    {
        ContextImpl*            context = {};
        VkDescriptorPool descriptorPool = {};
        VkRenderPass         renderPass = {};
        VkFramebuffer       framebuffer = {};

        Vec2U              lastSize = {};
        VkImageUsageFlags lastUsage = {};

        ImGuiContext*     imguiCtx = {};
        ImGuiContext* lastImguiCtx = {};

    public:
        ~ImGuiWrapperImpl();
    };

    struct ImGuiConfig
    {
        f32      uiScale = 1.5f;
        const char* font = "assets/fonts/CONSOLA.TTF";
        f32     fontSize = 20.f;
        Vec2 glyphOffset = Vec2(1.f, 1.67f);
        i32        flags = 0;
        u32   imageCount = 2;
    };

    struct ImGuiWrapper : ImplHandle<ImGuiWrapperImpl>
    {
    public:
        ImGuiWrapper() = default;
        ImGuiWrapper(Context context,
            CommandList cmd, Format format, GLFWwindow* window,
            const ImGuiConfig& config);

        void BeginFrame() const;
        void EndFrame(CommandList cmd, Texture texture) const;
    };
}