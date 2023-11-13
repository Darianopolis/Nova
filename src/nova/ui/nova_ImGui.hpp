#pragma once

#include <nova/rhi/nova_RHI.hpp>

#include <imgui.h>
#include <GLFW/glfw3.h>

namespace nova::imgui
{
    struct ImGuiConfig
    {
        f32      ui_scale = 1.5f;
        const char*  font = "assets/fonts/CONSOLA.TTF";
        f32     font_size = 20.f;
        Vec2 glyph_offset = Vec2(1.f, 1.67f);
        i32         flags = 0;

        GLFWwindow* window;

        Context context;
        Sampler sampler;
    };

    struct ImGuiLayer
    {
        Context context;

        ImGuiContext*      imgui_ctx = {};
        ImGuiContext* last_imgui_ctx = {};

        Sampler default_sampler;

        Texture font_texture;

        Shader   vertex_shader;
        Shader fragment_shader;

        Buffer vertex_buffer;
        Buffer  index_buffer;

        bool ended = false;

        bool dock_menu_bar = true;
        bool    no_dock_bg = true;

    public:
        ImGuiLayer(const ImGuiConfig& config);
        ~ImGuiLayer();

        ImTextureID GetTextureID(Texture texture)
        {
            return std::bit_cast<ImTextureID>(Vec2U(texture.GetDescriptor(), default_sampler.GetDescriptor()));
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