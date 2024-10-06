#pragma once

#include <nova/rhi/nova_RHI.hpp>
#include <nova/window/nova_Window.hpp>
#include <nova/vfs/nova_VirtualFilesystem.hpp>

#include <imgui.h>

namespace nova::imgui
{
    struct ImGuiConfig
    {
        f32             ui_scale = 1.5f;
        std::span<const b8> font = nova::vfs::Load("CONSOLA.TTF");
        f32            font_size = 20.f;
        Vec2        glyph_offset = Vec2(1.f, 1.67f);
        i32                flags = 0;

        Window window;

        Context context;
        Sampler sampler;

        u32 frames_in_flight = 0;
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

        struct FrameData
        {
            Buffer vertex_buffer;
            Buffer  index_buffer;
        };

        std::vector<FrameData> frame_data;
        u32 frames_in_flight;
        u32 frame_index = 0;

        std::chrono::steady_clock::time_point last_time = {};

        bool ended = false;

        bool dock_menu_bar = true;
        bool    no_dock_bg = false;

        struct TextureDescriptor
        {
            ImageSamplerDescriptor handle;
            u32                   padding = {};

            TextureDescriptor(ImageDescriptor image, SamplerDescriptor sampler)
                : handle(image, sampler)
            {}
        };

    public:
        ImGuiLayer(const ImGuiConfig& config);
        ~ImGuiLayer();

        ImTextureID TextureID(Image image, Sampler sampler = {}) const
        {
            return std::bit_cast<ImTextureID>(TextureDescriptor(image.Descriptor(), (sampler ? sampler : default_sampler).Descriptor()));
        }

        void BeginFrame(FunctionRef<void()> fn = []{});
        void EndFrame();

        bool HasDrawData();
        void DrawFrame(CommandList cmd, Image target);
    };
}