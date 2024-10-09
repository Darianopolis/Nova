#pragma once

#include "Draw2DShared.slang"

#include <nova/gpu/RHI.hpp>

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
        bool loaded = false;
    };

// -----------------------------------------------------------------------------

    struct Draw2D;
    struct Font;
    void FontDeleter(Font* font);

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

        Bounds2F bounds;

        std::vector<DrawCommand> draw_commands;
        std::vector<Font*> loaded_fonts;

    public:
        Draw2D(HContext);
        ~Draw2D();

        Sampler DefaultSampler() noexcept;
        const Bounds2F& Bounds() const noexcept;

        Font* LoadFont(Span<b8> data);

        void Reset();
        void DrawRect(const Rectangle& rect);
        void DrawString(StringView str, Vec2 pos, Font& font, f32 size);

        Bounds2F MeasureString(StringView str, Font& font, f32 size);

        void Record(CommandList, Image target);
    };
}
