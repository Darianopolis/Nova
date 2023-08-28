#pragma once

#include <nova/rhi/nova_RHI.hpp>
#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_SubAllocation.hpp>

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

        GLFWwindow* window;

        Context context;
        DescriptorHeap heap;
        DescriptorHandle sampler;
        DescriptorHandle fontTextureID;
    };

    struct ImGuiLayer
    {
        Context context = {};

        ImGuiContext*     imguiCtx = {};
        ImGuiContext* lastImguiCtx = {};

        DescriptorHeap heap;

        DescriptorHandle defaultSamplerID;

        Texture         fontTexture;
        DescriptorHandle fontTextureID;

        Shader vertexShader;
        Shader fragmentShader;
        Buffer vertexBuffer;
        Buffer indexBuffer;

        bool ended = false;

        bool dockMenuBar = true;
        bool noDockBg = true;

    public:
        ImGuiLayer(const ImGuiConfig& config,
            CommandPool cmdPool,
            CommandState cmdState,
            Queue queue,
            Fence fence);
        ~ImGuiLayer();

        ImTextureID GetTextureID(DescriptorHandle texture)
        {
            return ImTextureID(u64(defaultSamplerID.ToShaderUInt()) << 32 | texture.ToShaderUInt());
        }

        ImTextureID GetTextureID(DescriptorHandle texture, DescriptorHandle sampler)
        {
            return ImTextureID(u64(sampler.ToShaderUInt()) << 32 | texture.ToShaderUInt());
        }

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