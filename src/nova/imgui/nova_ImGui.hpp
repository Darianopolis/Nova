#pragma once

#include <nova/rhi/nova_RHI.hpp>

#include <GLFW/glfw3.h>

#include <imgui.h>

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

    struct ImGuiWrapper
    {
        virtual ~ImGuiWrapper() = default;

    protected:
        using DockspaceWindowFn = void(*)(void*, ImGuiWrapper&);
        virtual void BeginFrame_(DockspaceWindowFn fn, void* payload) = 0;

    public:
        template<typename Fn>
        void BeginFrame(Fn&& fn)
        {
            BeginFrame_(
                [](void* payload, ImGuiWrapper& self) { (*static_cast<Fn*>(payload))(self); },
                const_cast<Fn*>(&fn));
        }

        void BeginFrame()
        {
            BeginFrame_([](void*, ImGuiWrapper&) {}, nullptr);
        }

        // virtual void BeginFrame() = 0;

        virtual void EndFrame() = 0;

        virtual bool HasDrawData() = 0;
        virtual void DrawFrame(CommandList cmd, Texture texture) = 0;
    };
}