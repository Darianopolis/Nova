#pragma once

#include <nova/rhi/nova_RHI.hpp>

#include <nova/core/nova_SubAllocation.hpp>

namespace nova
{
    struct ImBounds2D
    {
        Vec2 min {  INFINITY,  INFINITY };
        Vec2 max { -INFINITY, -INFINITY };

        void Expand(const ImBounds2D& other) noexcept
        {
            min.x = std::min(min.x, other.min.x);
            min.y = std::min(min.y, other.min.y);
            max.x = std::max(max.x, other.max.x);
            max.y = std::max(max.y, other.max.y);
        }

        Vec2 Size()   const noexcept { return max - min; }
        Vec2 Center() const noexcept { return 0.5f * (max + min); }

        float Width()  const noexcept { return max.x - min.x; }
        float Height() const noexcept { return max.y - min.y; }

        bool Empty() const noexcept { return min.y == INFINITY; }
    };

    struct ImRoundRect
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

    enum class ImDrawType
    {
        RoundRect,
    };

    struct ImDrawCommand
    {
        ImDrawType type;
        u32       first;
        u32       count;
    };

// -----------------------------------------------------------------------------

    struct ImGlyph
    {
        Texture texture;

        f32   width;
        f32  height;
        f32 advance;
        Vec2 offset;
    };

// -----------------------------------------------------------------------------

    struct ImDraw2D;

    struct ImFont
    {
        ImDraw2D* im_draw;

        std::vector<ImGlyph> glyphs;

    public:
        ~ImFont();
    };

    struct ImDraw2D
    {

        static constexpr u32 MaxPrimitives = 65'536;

    public:
        Context context = {};

        Sampler default_sampler;

        Shader rect_vert_shader;
        Shader rect_frag_shader;
        Buffer      rect_buffer;
        u32          rect_index = 0;

        ImBounds2D bounds;

        std::vector<ImDrawCommand> draw_commands;

    public:
        ImDraw2D(HContext);
        ~ImDraw2D();

        Sampler GetDefaultSampler() noexcept;
        const ImBounds2D& GetBounds() const noexcept;

        std::unique_ptr<ImFont> LoadFont(const char* file, f32 size);

        void Reset();
        void DrawRect(const ImRoundRect& rect);
        void DrawString(std::string_view str, Vec2 pos, ImFont& font);

        ImBounds2D MeasureString(std::string_view str, ImFont& font);

        void Record(CommandList, Texture target);
    };
}