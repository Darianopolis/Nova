#pragma once

#include <nova/rhi/nova_RHI.hpp>
#include <nova/window/nova_Window.hpp>

#include <imgui.h>

namespace nova::imgui
{
    struct ImGuiConfig
    {
        f32      ui_scale = 1.5f;
        const char*  font = "assets/fonts/CONSOLA.TTF";
        f32     font_size = 20.f;
        Vec2 glyph_offset = Vec2(1.f, 1.67f);
        i32         flags = 0;

        Window window;

        Context context;
        Sampler sampler;
    };

    struct ImGuiLayer
    {
        Context context;

        Window window;

        ImGuiContext*      imgui_ctx = {};
        ImGuiContext* last_imgui_ctx = {};

        Sampler default_sampler;

        Image font_image;

        Shader   vertex_shader;
        Shader fragment_shader;

        Buffer vertex_buffer;
        Buffer  index_buffer;

        std::chrono::steady_clock::time_point last_time = {};

        bool ended = false;

        bool dock_menu_bar = true;
        bool    no_dock_bg = true;

    public:
        ImGuiLayer(const ImGuiConfig& config);
        ~ImGuiLayer();

        ImTextureID GetTextureID(Image image, Sampler sampler = {})
        {
            return std::bit_cast<ImTextureID>(Vec2U(image.GetDescriptor(), (sampler ? sampler : default_sampler).GetDescriptor()));
        }

        void BeginFrame(LambdaRef<void()> fn = []{});
        void EndFrame();

        bool HasDrawData();
        void DrawFrame(CommandList cmd, Image target, Fence fence);
    };
}