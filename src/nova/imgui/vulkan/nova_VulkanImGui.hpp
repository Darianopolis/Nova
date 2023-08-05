#pragma once

#include <nova/imgui/nova_ImGui.hpp>

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

namespace nova
{
    struct ImGuiLayer
    {
        HContext context = {};

        ImGuiContext*     imguiCtx = {};
        ImGuiContext* lastImguiCtx = {};

        bool ended = false;

    public:
        ImGuiLayer(HContext context,
            HCommandList cmd, Format format, GLFWwindow* window,
            const ImGuiConfig& config);

        ~ImGuiLayer();

        using DockspaceWindowFn = void(*)(void*, ImGuiLayer&);
        void BeginFrame_(DockspaceWindowFn fn, void* payload);
        void EndFrame();

        template<typename Fn>
        void BeginFrame(Fn&& fn)
        {
            BeginFrame_(
                [](void* payload, ImGuiLayer& self) { (*static_cast<Fn*>(payload))(self); },
                const_cast<Fn*>(&fn));
        }

        void BeginFrame()
        {
            BeginFrame_([](void*, ImGuiLayer&) {}, nullptr);
        }

        bool HasDrawData();
        void DrawFrame(HCommandList cmd, HTexture texture);
    };
}