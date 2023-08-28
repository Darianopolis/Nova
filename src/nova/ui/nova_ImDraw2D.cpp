#include "nova_ImDraw2D.hpp"

#define FT_CONFIG_OPTION_SUBPIXEL_RENDERING
#include <ft2build.h>
#include <freetype/freetype.h>

#include <freetype/ftlcdfil.h>

namespace nova
{
    ImDraw2D::ImDraw2D(HContext _context)
        : context(_context)
    {
        defaultSampler = nova::Sampler::Create(context, Filter::Linear,
            AddressMode::Border,
            BorderColor::TransparentBlack,
            16.f);

        descriptorHeap = nova::DescriptorHeap::Create(context, 65'536);
        descriptorHeap.WriteSampler(0, defaultSampler);

        rectVertShader = nova::Shader::Create(context, ShaderStage::Vertex, {
            nova::shader::Structure("ImRoundRect", ImRoundRect::Layout),
            nova::shader::Output("outTex", nova::ShaderVarType::Vec2),
            nova::shader::Output("outInstanceID", nova::ShaderVarType::U32),
            nova::shader::Fragment(R"glsl(
                const vec2[6] deltas = vec2[] (
                    vec2(-1, -1), vec2(-1,  1), vec2( 1, -1),
                    vec2(-1,  1), vec2( 1,  1), vec2( 1, -1));
            )glsl"),
            nova::shader::Kernel(R"glsl(
                uint instanceID = gl_VertexIndex / 6;
                uint vertexID = gl_VertexIndex % 6;

                ImRoundRect box = nova::BufferReference<ImRoundRect>(pc.rectInstancesVA).data[instanceID];
                vec2 delta = deltas[vertexID];
                outTex = delta * box.halfExtent;
                outInstanceID = instanceID;
                gl_Position = vec4(((delta * box.halfExtent) + box.centerPos - pc.centerPos) * pc.invHalfExtent, 0, 1);
            )glsl"),
        });

        rectFragShader = nova::Shader::Create(context, ShaderStage::Fragment, {
            nova::shader::Structure("ImRoundRect", ImRoundRect::Layout),
            nova::shader::Input("inTex", nova::ShaderVarType::Vec2),
            nova::shader::Input("inInstanceID", nova::ShaderVarType::U32, nova::ShaderInputFlags::Flat),
            nova::shader::Output("outColor", nova::ShaderVarType::Vec4),

            nova::shader::Kernel(R"glsl(
                ImRoundRect box = nova::BufferReference<ImRoundRect>(pc.rectInstancesVA).data[inInstanceID];

                vec2 absPos = abs(inTex);
                vec2 cornerFocus = box.halfExtent - vec2(box.cornerRadius);

                vec4 sampled = box.texTint.a > 0
                    ? box.texTint * texture(nova::Sampler2D(nonuniformEXT(box.texIndex)), 0),
                        (inTex / box.halfExtent) * box.texHalfExtent + box.texCenterPos)
                    : vec4(0);
                vec4 centerColor = vec4(
                    sampled.rgb * sampled.a + box.centerColor.rgb * (1 - sampled.a),
                    sampled.a + box.centerColor.a * (1 - sampled.a));

                if (absPos.x > cornerFocus.x && absPos.y > cornerFocus.y) {
                    float dist = length(absPos - cornerFocus);
                    if (dist > box.cornerRadius + 0.5) {
                        discard;
                    }

                    outColor = (dist > box.cornerRadius - box.borderWidth + 0.5)
                        ? vec4(box.borderColor.rgb, box.borderColor.a * (1 - max(0, dist - (box.cornerRadius - 0.5))))
                        : mix(centerColor, box.borderColor, max(0, dist - (box.cornerRadius - box.borderWidth - 0.5)));
                } else {
                    outColor = (absPos.x > box.halfExtent.x - box.borderWidth || absPos.y > box.halfExtent.y - box.borderWidth)
                        ? box.borderColor
                        : centerColor;
                }
            )glsl"),
        });

        rectBuffer = nova::Buffer::Create(context, sizeof(ImRoundRect) * MaxPrimitives,
            BufferUsage::Storage,
            BufferFlags::DeviceLocal | BufferFlags::Mapped);
    }

    ImDraw2D::~ImDraw2D()
    {}

// -----------------------------------------------------------------------------

    Sampler ImDraw2D::GetDefaultSampler() noexcept
    {
        return defaultSampler;
    }

    const ImBounds2D& ImDraw2D::GetBounds() const noexcept
    {
        return bounds;
    }

// -----------------------------------------------------------------------------

    DescriptorHandle ImDraw2D::RegisterTexture(Texture texture, Sampler sampler)
    {
        (void)sampler;// TODO: Handle custom sampler!
        auto handle = textureSlots.Acquire();
        descriptorHeap.WriteSampledTexture(handle, texture);
        return handle;
    }

    void ImDraw2D::UnregisterTexture(DescriptorHandle handle)
    {
        textureSlots.Release(handle.ToShaderUInt());
    }

// -----------------------------------------------------------------------------

    std::unique_ptr<ImFont> ImDraw2D::LoadFont(const char* file, f32 size, CommandPool cmdPool, CommandState state, Fence fence, Queue queue)
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

        auto font = std::make_unique<ImFont>();
        font->imDraw = this;

        auto staging = nova::Buffer::Create(context, u64(size) * u64(size) * 4,
            BufferUsage::TransferSrc, BufferFlags::Mapped);
        NOVA_CLEANUP(&) { staging.Destroy(); };

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

            glyph.texture = nova::Texture::Create(context,
                Vec3(f32(w), f32(h), 0.f),
                TextureUsage::Sampled,
                Format::RGBA8_UNorm);

            usz dataSize = w * h * 4;
            std::memcpy(staging.GetMapped(), pixels.data(), dataSize);

            auto cmd = cmdPool.Begin(state);
            cmd.CopyToTexture(glyph.texture, staging);
            cmd.GenerateMips(glyph.texture);
            queue.Submit({cmd}, {}, {fence});
            fence.Wait();

            glyph.index = RegisterTexture(glyph.texture, defaultSampler);
        }

        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        return font;
    }

    ImFont::~ImFont()
    {
        for (auto& glyph : glyphs) {
            if (glyph.texture) {
                imDraw->UnregisterTexture(glyph.index);
                glyph.texture.Destroy();
            }
        }
    }

    void ImDraw2D::Reset()
    {
        rectIndex = 0;
        bounds = {};

        drawCommands.clear();
    }

    void ImDraw2D::DrawRect(const ImRoundRect& rect)
    {
        ImDrawCommand& cmd = (drawCommands.size() && drawCommands.back().type == ImDrawType::RoundRect)
            ? drawCommands.back()
            : drawCommands.emplace_back(ImDrawType::RoundRect, rectIndex, 0);

        rectBuffer.Get<ImRoundRect>(cmd.first + cmd.count) = rect;

        bounds.Expand({{rect.centerPos - rect.halfExtent}, {rect.centerPos + rect.halfExtent}});

        cmd.count++;
    }

    void ImDraw2D::DrawString(std::string_view str, Vec2 pos, ImFont& font)
    {
        for (auto c : str) {
            auto& g = font.glyphs[c];

            DrawRect(nova::ImRoundRect {
                .centerPos = Vec2(g.width / 2.f, g.height / 2.f) + pos + Vec2(g.offset.x, -g.offset.y),
                .halfExtent = { g.width / 2.f, g.height / 2.f },
                .texTint = { 1.f, 1.f, 1.f, 1.f, },
                .texIndex = g.index,
                .texCenterPos = { 0.5f, 0.5f },
                .texHalfExtent = { 0.5f, 0.5f },
            });

            pos.x += g.advance;
        }
    }

    ImBounds2D ImDraw2D::MeasureString(std::string_view str, ImFont& font)
    {
        ImBounds2D strBounds = {};

        Vec2 pos = Vec2(0);

        for (auto c : str) {
            auto& g = font.glyphs[c];
            Vec2 centerPos = pos + Vec2(g.width / 2.f, g.height / 2.f) + Vec2(g.offset.x -g.offset.y);
            Vec2 halfExtent = Vec2(g.width / 2.f, g.height / 2.f);

            strBounds.Expand({{centerPos - halfExtent}, {centerPos + halfExtent}});

            pos.x += g.advance;
        }

        return strBounds;
    }

    void ImDraw2D::Record(CommandList cmd)
    {
        cmd.PushConstants(0, sizeof(PushConstants),
            Temp(PushConstants {
                .invHalfExtent = 2.f / bounds.Size(),
                .centerPos = bounds.Center(),
                .rectInstancesVA = rectBuffer.GetAddress(),
            }));

        cmd.BindDescriptorHeap(nova::BindPoint::Graphics, descriptorHeap);

        for (auto& command : drawCommands) {
            switch (command.type) {
            break;case ImDrawType::RoundRect:
                cmd.SetGraphicsState({rectVertShader, rectFragShader},
                    { .blendEnable = true });
                cmd.Draw(6 * command.count, 1, 6 * command.first, 0);
            }
        }
    }
}