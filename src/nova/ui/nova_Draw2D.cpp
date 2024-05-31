#include "nova_Draw2D.hpp"
#include "nova_Draw2D.slang"

#define FT_CONFIG_OPTION_SUBPIXEL_RENDERING
#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftlcdfil.h>

namespace nova::draw
{
    Draw2D::Draw2D(HContext _context)
        : context(_context)
    {
        default_sampler = Sampler::Create(context, Filter::Linear,
            AddressMode::Border,
            BorderColor::TransparentBlack,
            16.f);

        rect_vert_shader   = Shader::Create(context, ShaderLang::Slang, ShaderStage::Vertex,   "Vertex",   "nova/ui/nova_Draw2D.slang");
        rect_frag_shader = Shader::Create(context,   ShaderLang::Slang, ShaderStage::Fragment, "Fragment", "nova/ui/nova_Draw2D.slang");

        rect_buffer = Buffer::Create(context, sizeof(Rectangle) * MaxPrimitives,
            BufferUsage::Storage,
            BufferFlags::DeviceLocal | BufferFlags::Mapped);
    }

    Draw2D::~Draw2D()
    {
        rect_buffer.Destroy();
        rect_frag_shader.Destroy();
        rect_vert_shader.Destroy();
        default_sampler.Destroy();
    }

// -----------------------------------------------------------------------------

    Sampler Draw2D::DefaultSampler() noexcept
    {
        return default_sampler;
    }

    const Bounds2F& Draw2D::Bounds() const noexcept
    {
        return bounds;
    }

// -----------------------------------------------------------------------------

    std::unique_ptr<Font> Draw2D::LoadFont(std::span<const std::byte> data, f32 size)
    {
        // https://freetype.org/freetype2/docs/reference/ft2-lcd_rendering.html

        FT_Library ft;
        if (auto ec = FT_Init_FreeType(&ft)) {
            NOVA_THROW("Failed to init freetype - {}", int(ec));
        }

        FT_Face face;
        if (auto ec = FT_New_Memory_Face(ft, reinterpret_cast<const FT_Byte*>(data.data()), FT_Long(data.size()), 0, &face)) {
            NOVA_THROW("Failed to load font - {}", int(ec));
        }

        FT_Set_Pixel_Sizes(face, 0, u32(size));

        struct Pixel { u8 r, g, b, a; };
        std::vector<Pixel> pixels;

        auto font = std::make_unique<Font>();
        font->im_draw = this;

        font->glyphs.resize(128);
        for (u32 c = 0; c < 128; ++c) {
            FT_Load_Char(face, c, FT_LOAD_RENDER);
            u32 w = face->glyph->bitmap.width;
            u32 h = face->glyph->bitmap.rows;

            auto& glyph = font->glyphs[c];
            glyph.width = f32(w);
            glyph.height = f32(h);
            glyph.advance = face->glyph->advance.x / 64.f;
            glyph.offset = {
                face->glyph->bitmap_left,
                face->glyph->bitmap_top,
            };

            if (w == 0 || h == 0) {
                continue;
            }

            pixels.resize(w * h);
            for (u32 i = 0; i < w * h; ++i) {
                pixels[i] = { 255, 255, 255, face->glyph->bitmap.buffer[i] };
            }

            glyph.image = Image::Create(context,
                Vec3(f32(w), f32(h), 0.f),
                ImageUsage::Sampled,
                Format::RGBA8_UNorm);

            glyph.image.Set({}, glyph.image.Extent(), pixels.data());
            glyph.image.Transition(ImageLayout::Sampled);
        }

        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        return font;
    }

    Font::~Font()
    {
        for (auto& glyph : glyphs) {
            if (glyph.image) {
                glyph.image.Destroy();
            }
        }
    }

    void Draw2D::Reset()
    {
        rect_index = 0;
        bounds = {};

        draw_commands.clear();
    }

    void Draw2D::DrawRect(const Rectangle& rect)
    {
        DrawCommand& cmd = (draw_commands.size() && draw_commands.back().type == DrawType::RoundRect)
            ? draw_commands.back()
            : draw_commands.emplace_back(DrawType::RoundRect, rect_index, 0);

        rect_buffer.Get<Rectangle>(cmd.first + cmd.count) = rect;

        bounds.Expand({{rect.center_pos - rect.half_extent}, {rect.center_pos + rect.half_extent}});

        cmd.count++;
    }

    void Draw2D::DrawString(StringView str, Vec2 pos, Font& font)
    {
        for (auto c : str) {
            u8 uc = u8(c);
            // TODO: Unicode!
            if (uc >= 128) uc = 0;
            auto& g = font.glyphs[uc];

            if (g.image) {
                DrawRect(Rectangle {
                    .center_pos = Vec2(g.width / 2.f, g.height / 2.f) + pos + Vec2(g.offset.x, -g.offset.y),
                    .half_extent = { g.width / 2.f, g.height / 2.f },
                    .tex_tint = { 1.f, 1.f, 1.f, 1.f, },
                    .tex_handle = {g.image.Descriptor(), default_sampler.Descriptor()},
                    .tex_center_pos = { 0.5f, 0.5f },
                    .tex_half_extent = { 0.5f, 0.5f },
                });
            }

            pos.x += g.advance;
        }
    }

    Bounds2F Draw2D::MeasureString(StringView str, Font& font)
    {
        Bounds2F str_bounds = {};

        Vec2 pos = Vec2(0);

        for (auto c : str) {
            auto& g = font.glyphs[c];
            Vec2 center_pos = pos + Vec2(g.width / 2.f, g.height / 2.f) + Vec2(g.offset.x -g.offset.y);
            Vec2 half_extent = Vec2(g.width / 2.f, g.height / 2.f);

            str_bounds.Expand({{center_pos - half_extent}, {center_pos + half_extent}});

            pos.x += g.advance;
        }

        return str_bounds;
    }

    void Draw2D::Record(CommandList cmd, Image target)
    {
        cmd.ResetGraphicsState();
        cmd.BeginRendering({
            .region = {{}, Vec2U(target.Extent())},
            .color_attachments = {target}
        });
        cmd.SetViewports({{{}, Vec2I(target.Extent())}}, true);
        cmd.SetBlendState({true});

        cmd.PushConstants(PushConstants {
            .inv_half_extent = 2.f / bounds.Size(),
            .center_pos = bounds.Center(),
            .rects = (Rectangle*)rect_buffer.DeviceAddress(),
        });

        for (auto& command : draw_commands) {
            switch (command.type) {
                break;case DrawType::RoundRect:
                    cmd.BindShaders({rect_vert_shader, rect_frag_shader});
                    cmd.Draw(6 * command.count, 1, 6 * command.first, 0);
            }
        }

        cmd.EndRendering();
    }
}