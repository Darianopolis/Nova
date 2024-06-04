#pragma once

#include "nova_Draw2DShared.slang"

#include <nova/rhi/nova_RHI.hpp>

namespace nova::draw
{
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
        Image image;

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
        Sampler default_sampler;

        Shader rect_vert_shader;
        Shader rect_frag_shader;
        Buffer      rect_buffer;
        u32          rect_index = 0;

        Bounds2F bounds;

        std::vector<DrawCommand> draw_commands;

    public:
        Draw2D();
        ~Draw2D();

        Sampler DefaultSampler() noexcept;
        const Bounds2F& Bounds() const noexcept;

        std::unique_ptr<Font> LoadFont(std::span<const std::byte> data, f32 size);

        void Reset();
        void DrawRect(const Rectangle& rect);
        void DrawString(StringView str, Vec2 pos, Font& font);

        Bounds2F MeasureString(StringView str, Font& font);

        void Record(CommandList, Image target);
    };
}