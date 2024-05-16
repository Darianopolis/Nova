#include "nova_Draw2D.hpp"

#define FT_CONFIG_OPTION_SUBPIXEL_RENDERING
#include <ft2build.h>
#include <freetype/freetype.h>

#include <freetype/ftlcdfil.h>

namespace nova::draw
{
    namespace {
        struct PushConstants
        {
            Vec2  inv_half_extent;
            Vec2      center_pos;
            u64           rects;
            u32    sampler_index;
        };

        static
        constexpr auto Preamble = R"glsl(
#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_buffer_reference2    : require
#extension GL_EXT_nonuniform_qualifier : require
expl
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer ImRoundRect {
    vec4 center_color;
    vec4 border_color;

    vec2  center_pos;
    vec2 half_extent;

    float corner_radius;
    float  border_width;

    vec4        tex_tint;
    uint       tex_index;
    vec2  tex_center_pos;
    vec2 tex_half_extent;
};

layout(push_constant, scalar) readonly uniform pc_ {
    vec2 inv_half_extent;
    vec2      center_pos;
    ImRoundRect    rects;
    uint   sampler_index;
} pc;
        )glsl"sv;
    }

    Draw2D::Draw2D(HContext _context)
        : context(_context)
    {
        default_sampler = Sampler::Create(context, Filter::Linear,
            AddressMode::Border,
            BorderColor::TransparentBlack,
            16.f);

        rect_vert_shader = Shader::Create(context,
            ShaderLang::Glsl, ShaderStage::Vertex, "main", "", {
                Preamble,
                R"glsl(
const vec2[6] deltas = vec2[] (
    vec2(-1, -1), vec2(-1,  1), vec2( 1, -1),
    vec2(-1,  1), vec2( 1,  1), vec2( 1, -1));

layout(location = 0) out vec2 out_tex;
layout(location = 1) out uint out_instance_id;

void main() {
    uint instance_id = gl_VertexIndex / 6;
    uint vertex_id = gl_VertexIndex % 6;

    ImRoundRect box = pc.rects[instance_id];
    vec2 delta = deltas[vertex_id];
    out_tex = delta * box.half_extent;
    out_instance_id = instance_id;
    gl_Position = vec4(((delta * box.half_extent) + box.center_pos - pc.center_pos) * pc.inv_half_extent, 0, 1);
}
                )glsl"
            });

        rect_frag_shader = Shader::Create(context,
            ShaderLang::Glsl, ShaderStage::Fragment, "main", "", {
                Preamble,
                R"glsl(
layout(set = 0, binding = 0) uniform texture2D Image2D[];
layout(set = 0, binding = 2) uniform sampler Sampler[];

layout(location = 0) in vec2 in_tex;
layout(location = 1) in flat uint in_instance_id;
layout(location = 0) out vec4 out_color;

void main() {
    ImRoundRect box = pc.rects[in_instance_id];

    vec2 abs_pos = abs(in_tex);
    vec2 corner_focus = box.half_extent - vec2(box.corner_radius);

    vec4 sampled = box.tex_tint.a > 0
        ? box.tex_tint * texture(sampler2D(Image2D[nonuniformEXT(box.tex_index)], Sampler[0]),
            (in_tex / box.half_extent) * box.tex_half_extent + box.tex_center_pos)
        : vec4(0);
    vec4 center_color = vec4(
        sampled.rgb * sampled.a + box.center_color.rgb * (1 - sampled.a),
        sampled.a + box.center_color.a * (1 - sampled.a));

    if (abs_pos.x > corner_focus.x && abs_pos.y > corner_focus.y) {
        float dist = length(abs_pos - corner_focus);
        if (dist > box.corner_radius + 0.5) {
            discard;
        }

        out_color = (dist > box.corner_radius - box.border_width + 0.5)
            ? vec4(box.border_color.rgb, box.border_color.a * (1 - max(0, dist - (box.corner_radius - 0.5))))
            : mix(center_color, box.border_color, max(0, dist - (box.corner_radius - box.border_width - 0.5)));
    } else {
        out_color = (abs_pos.x > box.half_extent.x - box.border_width || abs_pos.y > box.half_extent.y - box.border_width)
            ? box.border_color
            : center_color;
    }
}
                )glsl"
            });

        rect_buffer = Buffer::Create(context, sizeof(Rectangle) * MaxPrimitives,
            BufferUsage::Storage,
            BufferFlags::DeviceLocal | BufferFlags::Mapped);
    }

    Draw2D::~Draw2D()
    {}

// -----------------------------------------------------------------------------

    Sampler Draw2D::DefaultSampler() noexcept
    {
        return default_sampler;
    }

    const Bounds2& Draw2D::Bounds() const noexcept
    {
        return bounds;
    }

// -----------------------------------------------------------------------------

    std::unique_ptr<Font> Draw2D::LoadFont(const char* file, f32 size)
    {
        // https://freetype.org/freetype2/docs/reference/ft2-lcd_rendering.html

        FT_Library ft;
        if (auto ec = FT_Init_FreeType(&ft)) {
            NOVA_THROW("Failed to init freetype - {}", int(ec));
        }

        FT_Face face;
        if (auto ec = FT_New_Face(ft, file, 0, &face)) {
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
            auto& g = font.glyphs[c];

            if (g.image) {
                DrawRect(Rectangle {
                    .center_pos = Vec2(g.width / 2.f, g.height / 2.f) + pos + Vec2(g.offset.x, -g.offset.y),
                    .half_extent = { g.width / 2.f, g.height / 2.f },
                    .tex_tint = { 1.f, 1.f, 1.f, 1.f, },
                    .tex_idx = g.image.Descriptor(),
                    .tex_center_pos = { 0.5f, 0.5f },
                    .tex_half_extent = { 0.5f, 0.5f },
                });
            }

            pos.x += g.advance;
        }
    }

    Bounds2 Draw2D::MeasureString(StringView str, Font& font)
    {
        Bounds2 str_bounds = {};

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
            .rects = rect_buffer.DeviceAddress(),
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