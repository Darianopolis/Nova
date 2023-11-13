#pragma once

#include <nova/rhi/nova_RHI.hpp>

namespace nova::draw
{
    struct Rectangle
    {
        Vec4 center_color;
        Vec4 border_color;

        Vec2  center_pos;
        Vec2 half_extent;

        f32 corner_radius;
        f32  border_width;

        Vec4        tex_tint;
        u32          tex_idx;
        Vec2  tex_center_pos;
        Vec2 tex_half_extent;
    };

    enum class DrawType
    {
        RoundRect,
    };

    struct DrawCommand
    {
        DrawType type;
        u32       first;
        u32       count;
    };

// -----------------------------------------------------------------------------

    struct Glyph
    {
        Texture texture;

        f32   width;
        f32  height;
        f32 advance;
        Vec2 offset;
    };

// -----------------------------------------------------------------------------

    struct Draw2D;

    struct Font
    {
        Draw2D* im_draw;

        std::vector<Glyph> glyphs;

    public:
        ~Font();
    };

    struct Draw2D
    {

        static constexpr u32 MaxPrimitives = 65'536;

    public:
        Context context = {};

        Sampler default_sampler;

        Shader rect_vert_shader;
        Shader rect_frag_shader;
        Buffer      rect_buffer;
        u32          rect_index = 0;

        Bounds2 bounds;

        std::vector<DrawCommand> draw_commands;

    public:
        Draw2D(HContext);
        ~Draw2D();

        Sampler GetDefaultSampler() noexcept;
        const Bounds2& GetBounds() const noexcept;

        std::unique_ptr<Font> LoadFont(const char* file, f32 size);

        void Reset();
        void DrawRect(const Rectangle& rect);
        void DrawString(std::string_view str, Vec2 pos, Font& font);

        Bounds2 MeasureString(std::string_view str, Font& font);

        void Record(CommandList, Texture target);
    };
}