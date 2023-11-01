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

        GLFWwindow* window;

        Context context;
        Sampler sampler;
    };

    struct ImGuiLayer
    {
        Context context;

        ImGuiContext*     imguiCtx = {};
        ImGuiContext* lastImguiCtx = {};

        Sampler defaultSampler;

        Texture fontTexture;

        Shader   vertexShader;
        Shader fragmentShader;

        Buffer vertexBuffer;
        Buffer  indexBuffer;

        bool ended = false;

        bool dockMenuBar = true;
        bool    noDockBg = true;

    public:
        ImGuiLayer(const ImGuiConfig& config);
        ~ImGuiLayer();

        ImTextureID GetTextureID(Texture texture)
        {
            return std::bit_cast<ImTextureID>(Vec2U(texture.GetDescriptor(), defaultSampler.GetDescriptor()));
        }

        ImTextureID GetTextureID(Texture texture, Sampler sampler)
        {
            return std::bit_cast<ImTextureID>(Vec2U(texture.GetDescriptor(), sampler.GetDescriptor()));
        }

        void BeginFrame(LambdaRef<void()> fn = []{});
        void EndFrame();

        bool HasDrawData();
        void DrawFrame(CommandList cmd, Texture target, Fence fence);
    };
}