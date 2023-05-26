#include "nova_ImDraw2D.hpp"

#define FT_CONFIG_OPTION_SUBPIXEL_RENDERING
#include <ft2build.h>
#include <freetype/freetype.h>

#include <freetype/ftlcdfil.h>

namespace nova
{
    ImDraw2D* ImDraw2D::Create(Context* context)
    {
        auto imDraw = new ImDraw2D;
        imDraw->context = context;

// -----------------------------------------------------------------------------

        VkCall(vkCreateDescriptorSetLayout(context->device, Temp(VkDescriptorSetLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = Temp(VkDescriptorSetLayoutBindingFlagsCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = 1,
                .pBindingFlags = Temp<VkDescriptorBindingFlags>(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT),
            }),
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
            .bindingCount = 1,
            .pBindings = Temp(VkDescriptorSetLayoutBinding {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 65'536,
                .stageFlags = VK_SHADER_STAGE_ALL,
            }),
        }), nullptr, &imDraw->descriptorSetLayout));

        VkDeviceSize descriptorSize;
        vkGetDescriptorSetLayoutSizeEXT(context->device, imDraw->descriptorSetLayout, &descriptorSize);
        imDraw->descriptorBuffer = context->CreateBuffer(
            descriptorSize,
            BufferUsage::DescriptorSamplers,
            BufferFlags::DeviceLocal | BufferFlags::CreateMapped);

        VkCall(vkCreatePipelineLayout(context->device, Temp(VkPipelineLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &imDraw->descriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &PushConstantsRange,
        }), context->pAlloc, &imDraw->pipelineLayout));

// -----------------------------------------------------------------------------

        imDraw->rectBuffer = context->CreateBuffer(sizeof(ImRoundRect) * MaxPrimitives,
            BufferUsage::Storage,
            BufferFlags::DeviceLocal | BufferFlags::CreateMapped);

// -----------------------------------------------------------------------------

        std::string preamble = R"(
#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_nonuniform_qualifier : require

struct ImRoundRect
{
    vec4 centerColor;
    vec4 borderColor;

    vec2 centerPos;
    vec2 halfExtent;

    float cornerRadius;
    float borderWidth;

    vec4 texTint;
    uint texIndex;
    vec2 texCenterPos;
    vec2 texHalfExtent;
};
layout(buffer_reference, scalar) buffer ImRoundRectRef { ImRoundRect data[]; };

layout(push_constant) uniform PushConstants {
    vec2 invHalfExtent;
    vec2 centerPos;
    uint64_t rectInstancesVA;
} pc;
    )";

    imDraw->rectVertShader = context->CreateShader(
        ShaderStage::Vertex, {},
        "vertex",
        preamble + R"(
const vec2[6] deltas = vec2[] (
    vec2(-1, -1), vec2(-1,  1), vec2( 1, -1),
    vec2(-1,  1), vec2( 1,  1), vec2( 1, -1)
);

layout(location = 0) out vec2 outTex;
layout(location = 1) out uint outInstanceID;

void main()
{
    uint instanceID = gl_VertexIndex / 6;
    uint vertexID = gl_VertexIndex % 6;

    ImRoundRect box = ImRoundRectRef(pc.rectInstancesVA).data[instanceID];
    vec2 delta = deltas[vertexID];
    outTex = delta * box.halfExtent;
    outInstanceID = instanceID;
    gl_Position = vec4(((delta * box.halfExtent) + box.centerPos - pc.centerPos) * pc.invHalfExtent, 0, 1);
}
        )",
        {PushConstantsRange},
        {imDraw->descriptorSetLayout});

    imDraw->rectFragShader = context->CreateShader(
        ShaderStage::Fragment, {},
        "fragment",
        preamble + R"(
layout(location = 0) in vec2 inTex;
layout(location = 1) in flat uint inInstanceID;

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) out vec4 outColor;

void main()
{
    ImRoundRect box = ImRoundRectRef(pc.rectInstancesVA).data[inInstanceID];

    vec2 absPos = abs(inTex);
    vec2 cornerFocus = box.halfExtent - vec2(box.cornerRadius);

    vec4 sampled = box.texTint.a > 0
        ? box.texTint * texture(textures[nonuniformEXT(box.texIndex)], (inTex / box.halfExtent) * box.texHalfExtent + box.texCenterPos)
        : vec4(0);
    vec4 centerColor = vec4(
        sampled.rgb * sampled.a + box.centerColor.rgb * (1 - sampled.a),
        sampled.a + box.centerColor.a * (1 - sampled.a));

    if (absPos.x > cornerFocus.x && absPos.y > cornerFocus.y)
    {
        float dist = length(absPos - cornerFocus);
        if (dist > box.cornerRadius + 0.5)
            discard;

        outColor = (dist > box.cornerRadius - box.borderWidth + 0.5)
            ? vec4(box.borderColor.rgb, box.borderColor.a * (1 - max(0, dist - (box.cornerRadius - 0.5))))
            : mix(centerColor, box.borderColor, max(0, dist - (box.cornerRadius - box.borderWidth - 0.5)));
    }
    else
    {
        outColor = (absPos.x > box.halfExtent.x - box.borderWidth || absPos.y > box.halfExtent.y - box.borderWidth)
            ? box.borderColor
            : centerColor;
    }
}
        )",
        {PushConstantsRange},
        {imDraw->descriptorSetLayout});

// -----------------------------------------------------------------------------

        VkCall(vkCreateSampler(context->device, Temp(VkSamplerCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 16.f,
            .minLod = -1000.f,
            .maxLod = 1000.f,
            .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        }), nullptr, &imDraw->defaultSampler));

        return imDraw;
    }

    void ImDraw2D::Destroy(ImDraw2D* imDraw)
    {
        imDraw->context->DestroyBuffer(imDraw->rectBuffer);
        imDraw->context->DestroyShader(imDraw->rectVertShader);
        imDraw->context->DestroyShader(imDraw->rectFragShader);
        vkDestroyPipelineLayout(imDraw->context->device, imDraw->pipelineLayout, imDraw->context->pAlloc);

        delete imDraw;
    }

// -----------------------------------------------------------------------------

    ImTextureID ImDraw2D::RegisterTexture(Image* image, VkSampler sampler)
    {
        u32 index;
        if (textureSlotFreelist.empty())
        {
            index = nextTextureSlot++;
        }
        else
        {
            index = textureSlotFreelist.back();
            textureSlotFreelist.pop_back();
        }

        vkGetDescriptorEXT(context->device,
            Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .data = {
                    .pCombinedImageSampler = Temp(VkDescriptorImageInfo {
                        .sampler = sampler,
                        .imageView = image->view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    }),
                },
            }),
            context->descriptorSizes.combinedImageSamplerDescriptorSize,
            descriptorBuffer->mapped + (index * context->descriptorSizes.combinedImageSamplerDescriptorSize));

        return ImTextureID(index);
    }

    void ImDraw2D::UnregisterTexture(ImTextureID textureSlot)
    {
        textureSlotFreelist.push_back(u32(textureSlot));
    }

// -----------------------------------------------------------------------------

    ImFont* ImDraw2D::LoadFont(const char* file, f32 size, CommandPool* cmdPool, ResourceTracker* tracker, Fence* fence, Queue* queue)
    {
        // https://freetype.org/freetype2/docs/reference/ft2-lcd_rendering.html

        FT_Library ft;
        if (auto ec = FT_Init_FreeType(&ft))
            NOVA_THROW("Failed to init freetype - {}", int(ec));

        FT_Face face;
        if (auto ec = FT_New_Face(ft, file, 0, &face))
            NOVA_THROW("Failed to load font - {}", int(ec));

        FT_Set_Pixel_Sizes(face, 0, u32(size));

        struct Pixel { u8 r, g, b, a; };
        std::vector<Pixel> pixels;

        auto font = new ImFont;
        NOVA_ON_SCOPE_FAILURE(&) { DestroyFont(font); };

        auto staging = context->CreateBuffer(u64(size) * u64(size) * 4,
            BufferUsage::TransferSrc,
            BufferFlags::CreateMapped);

        font->glyphs.resize(128);
        for (u32 c = 0; c < 128; ++c)
        {
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

            if (w == 0 || h == 0)
                continue;

            pixels.resize(w * h);
            for (u32 i = 0; i < w * h; ++i)
                pixels[i] = { 255, 255, 255, face->glyph->bitmap.buffer[i] };

            glyph.image = context->CreateImage(
                Vec3(f32(w), f32(h), 0.f),
                ImageUsage::Sampled,
                Format::RGBA8U);

            usz dataSize = w * h * 4;
            std::memcpy(staging->mapped, pixels.data(), dataSize);

            auto cmd = cmdPool->BeginPrimary(tracker);
            cmd->CopyToImage(glyph.image, staging);
            cmd->GenerateMips(glyph.image);
            queue->Submit({cmd}, {}, {fence});
            fence->Wait();

            glyph.index = RegisterTexture(glyph.image, defaultSampler);
        }

        context->DestroyBuffer(staging);

        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        return font;
    }

    void ImDraw2D::DestroyFont(ImFont* font)
    {
        for (auto& glyph : font->glyphs)
        {
            if (glyph.image)
            {
                UnregisterTexture(glyph.index);
                context->DestroyImage(glyph.image);
            }
        }

        delete font;
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

        rectBuffer->Get<ImRoundRect>(cmd.first + cmd.count) = rect;

        bounds.Expand({{rect.centerPos - rect.halfExtent}, {rect.centerPos + rect.halfExtent}});

        cmd.count++;
    }

    void ImDraw2D::DrawString(std::string_view str, Vec2 pos, ImFont* font)
    {
        for (auto c : str)
        {
            auto& g = font->glyphs[c];

            DrawRect(nova::ImRoundRect {
                // .centerColor = { 0.f, 1.f, 0.f, 0.4f },
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

    ImBounds2D ImDraw2D::MeasureString(std::string_view str, ImFont* font)
    {
        ImBounds2D strBounds = {};

        Vec2 pos = Vec2(0);

        for (auto c : str)
        {
            auto& g = font->glyphs[c];
            Vec2 centerPos = pos + Vec2(g.width / 2.f, g.height / 2.f) + Vec2(g.offset.x -g.offset.y);
            Vec2 halfExtent = Vec2(g.width / 2.f, g.height / 2.f);

            strBounds.Expand({{centerPos - halfExtent}, {centerPos + halfExtent}});

            pos.x += g.advance;
        }

        return strBounds;
    }

    void ImDraw2D::Record(CommandList* cmd)
    {
        cmd->SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        cmd->PushConstants(pipelineLayout,
            ShaderStage::Vertex | ShaderStage::Fragment,
            PushConstantsRange.offset, PushConstantsRange.size,
            Temp(PushConstants {
                .invHalfExtent = 2.f / bounds.Size(),
                .centerPos = bounds.Center(),
                .rectInstancesVA = rectBuffer->address,
            }));

        cmd->SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        vkCmdBindDescriptorBuffersEXT(cmd->buffer, 1, Temp(VkDescriptorBufferBindingInfoEXT {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
            .address = descriptorBuffer->address,
            .usage = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
        }));
        vkCmdSetDescriptorBufferOffsetsEXT(cmd->buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, Temp(0u), Temp(0ull));

        for (auto& command : drawCommands)
        {
            switch (command.type)
            {
            break;case ImDrawType::RoundRect:
                cmd->BindShaders({rectVertShader, rectFragShader});
                cmd->Draw(6 * command.count, 1, 6 * command.first, 0);
            }
        }
    }
}