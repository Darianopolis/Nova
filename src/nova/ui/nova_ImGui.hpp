#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/rhi/nova_RHI.hpp>

#include <imgui.h>
#include <GLFW/glfw3.h>

namespace nova
{
    struct ImGuiConfig
    {
        f32      uiScale = 1.5f;
        const char* font = "assets/fonts/CONSOLA.TTF";
        f32     fontSize = 20.f;
        Vec2 glyphOffset = Vec2(1.f, 1.67f);
        i32        flags = 0;
        u32   imageCount = 2;
    };

    struct ImGuiLayer
    {
        Context context = {};

        ImGuiContext*     imguiCtx = {};
        ImGuiContext* lastImguiCtx = {};

        void* descriptorPool; // TODO: Move internal data to impl

        bool ended = false;

        bool dockMenuBar = true;
        bool noDockBg = true;

    public:
        ImGuiLayer(Context context,
            CommandList cmd, Format format, GLFWwindow* window,
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
        void DrawFrame(CommandList cmd, Texture texture);
    };
}