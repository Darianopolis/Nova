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

    public:
        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;

        virtual bool HasDrawData() = 0;
        virtual void DrawFrame(CommandList cmd, Texture texture) = 0;
    };
}