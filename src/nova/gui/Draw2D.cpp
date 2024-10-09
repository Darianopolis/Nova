#include "Draw2D.hpp"
#include "Draw2DRect.slang"

#define FT_CONFIG_OPTION_SUBPIXEL_RENDERING
#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftlcdfil.h>
#include <freetype/ftsizes.h>

namespace nova::draw
{
    struct GlyphKey
    {
        uint32_t    code;
        uint32_t    size;

        NOVA_MEMORY_EQUALITY_MEMBER(GlyphKey)
    };
}

NOVA_MEMORY_HASH(nova::draw::GlyphKey)

namespace nova::draw
{
    struct Font
    {
        Draw2D* im_draw;

        FT_Library ft;
        FT_Face  face;

        ankerl::unordered_dense::map<uint32_t, FT_Size> sizes;
        ankerl::unordered_dense::map<GlyphKey, Glyph>  glyphs;

        struct Pixel { u8 r, g, b, a; };
        std::vector<Pixel> pixels;

        Font(Draw2D* _im_draw, Span<b8> data)
            : im_draw(_im_draw)
        {
            // https://freetype.org/freetype2/docs/reference/ft2-lcd_rendering.html

            if (auto ec = FT_Init_FreeType(&ft)
                   ; ec != FT_Err_Ok) {
                NOVA_THROW("Failed to init freetype - {}", int(ec));
            }

            if (auto ec = FT_New_Memory_Face(ft, reinterpret_cast<const FT_Byte*>(data.data()), FT_Long(data.size()), 0, &face)
                   ; ec != FT_Err_Ok) {
                NOVA_THROW("Failed to load font - {}", int(ec));
            }
        }

        ~Font()
        {
            for (auto&[_, size] : sizes) {
                FT_Done_Size(size);
            }

            FT_Done_Face(face);
            FT_Done_FreeType(ft);

            for (auto&[key, glyph] : glyphs) {
                glyph.image.Destroy();
            }
        }

        const Glyph& GetGlyphForCodepoint(char32_t codepoint, uint32_t size)
        {
            // TODO: Should be two level, like FT

            GlyphKey key = {};
            key.code = codepoint;
            key.size = size;

            auto& glyph = glyphs[key];
            if (glyph.loaded) {
                return glyph;
            }

            glyph.loaded = true;

            auto& ft_size = sizes[size];
            if (!ft_size) {
                FT_New_Size(face, &ft_size);
                FT_Activate_Size(ft_size);
                FT_Set_Pixel_Sizes(face, 0, u32(size));
            } else {
                FT_Activate_Size(ft_size);
            }

            FT_Vector vec;
            vec.x = 64 / 2;
            vec.y = 64 / 2;
            FT_Set_Transform(face, nullptr, &vec);

            if (FT_Err_Ok != FT_Load_Char(face, FT_ULong(codepoint), FT_LOAD_RENDER)) {
                // TODO: Fallback fonts and/or unicode missing glyph
                NOVA_ASSERT(codepoint != 0, "codepoint 0 failed to load!");
                nova::Log("  char \"{}\" ({}) not found, falling back to codepoint 0", FromUtf32({&codepoint, 1}), uint32_t(codepoint));
                return GetGlyphForCodepoint(0, size);
            }

            u32 w = face->glyph->bitmap.width;
            u32 h = face->glyph->bitmap.rows;

            glyph.width = f32(w);
            glyph.height = f32(h);
            glyph.advance = face->glyph->advance.x / 64.f;
            glyph.offset = {
                face->glyph->bitmap_left,
                face->glyph->bitmap_top,
            };

            if (w == 0 || h == 0) {
                return glyph;
            }

            pixels.resize(w * h);
            for (u32 i = 0; i < w * h; ++i) {
                pixels[i] = { 255, 255, 255, face->glyph->bitmap.buffer[i] };
            }

            glyph.image = Image::Create(im_draw->context,
                Vec3(f32(w), f32(h), 0.f),
                ImageUsage::Sampled,
                Format::RGBA8_UNorm);

            glyph.image.Set({}, glyph.image.Extent(), pixels.data());
            glyph.image.Transition(ImageLayout::Sampled);

            return glyph;
        }
    };

// -----------------------------------------------------------------------------

    Draw2D::Draw2D(HContext _context)
        : context(_context)
    {
        default_sampler = Sampler::Create(context, Filter::Linear,
            AddressMode::Border,
            BorderColor::TransparentBlack,
            16.f);
        NOVA_CLEANUP_ON_EXCEPTION(&) { default_sampler.Destroy(); };

        rect_vert_shader = Shader::Create(context, ShaderLang::Slang, ShaderStage::Vertex,   "Vertex",   "nova/ui/Draw2DRect.slang");
        NOVA_CLEANUP_ON_EXCEPTION(&) { rect_vert_shader.Destroy(); };

        rect_frag_shader = Shader::Create(context, ShaderLang::Slang, ShaderStage::Fragment, "Fragment", "nova/ui/Draw2DRect.slang");
        NOVA_CLEANUP_ON_EXCEPTION(&) { rect_frag_shader.Destroy(); };

        rect_buffer = Buffer::Create(context, sizeof(Rectangle) * MaxPrimitives,
            BufferUsage::Storage,
            BufferFlags::DeviceLocal | BufferFlags::Mapped);
        NOVA_CLEANUP_ON_EXCEPTION(&) { rect_buffer.Destroy(); };
    }

    Draw2D::~Draw2D()
    {
        rect_buffer.Destroy();
        rect_frag_shader.Destroy();
        rect_vert_shader.Destroy();
        default_sampler.Destroy();
        for (auto* font : loaded_fonts) {
            delete font;
        }
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

    Font* Draw2D::LoadFont(Span<b8> data)
    {
        auto font = new Font(this, data);
        loaded_fonts.emplace_back(font);
        return font;
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

    void Draw2D::DrawString(StringView str, Vec2 _pos, Font& font, f32 size)
    {
        for (c32 c : ToUtf32(str)) {
            auto pos = glm::roundEven(_pos);
            auto& g = font.GetGlyphForCodepoint(c, u32(size));

            if (g.image) {
                DrawRect(Rectangle {
                    .center_pos = Vec2(g.width / 2.f, g.height / 2.f) + pos + Vec2(g.offset.x, -g.offset.y),
                    .half_extent = { g.width / 2.f, g.height / 2.f },
                    .tex_handle = {g.image.Descriptor(), default_sampler.Descriptor()},
                    .tex_tint = { 1.f, 1.f, 1.f, 1.f, },
                    .tex_center_pos = { 0.5f, 0.5f },
                    .tex_half_extent = { 0.5f, 0.5f },
                });
            }

            _pos.x += g.advance;
        }
    }

    Bounds2F Draw2D::MeasureString(StringView str, Font& font, f32 size)
    {
        Bounds2F str_bounds = {};

        Vec2 _pos = Vec2(0);

        for (auto c : ToUtf32(str)) {
            auto pos = glm::roundEven(_pos);
            auto& g = font.GetGlyphForCodepoint(c, u32(size));

            Vec2 center_pos = pos + Vec2(g.width / 2.f, g.height / 2.f) + Vec2(g.offset.x -g.offset.y);
            Vec2 half_extent = Vec2(g.width / 2.f, g.height / 2.f);

            str_bounds.Expand({{center_pos - half_extent}, {center_pos + half_extent}});

            _pos.x += g.advance;
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
