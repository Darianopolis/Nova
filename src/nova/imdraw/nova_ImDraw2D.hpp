#pragma once

#include <nova/rhi/nova_RHI.hpp>

namespace nova
{
    enum class ImTextureID : u32 {};

    struct ImBounds2D
    {
        Vec2 min {  INFINITY,  INFINITY };
        Vec2 max { -INFINITY, -INFINITY };

        void Expand(const ImBounds2D& other)
        {
            min.x = std::min(min.x, other.min.x);
            min.y = std::min(min.y, other.min.y);
            max.x = std::max(max.x, other.max.x);
            max.y = std::max(max.y, other.max.y);
        }

        Vec2 Size() const
        {
            return max - min;
        }

        Vec2 Center() const
        {
            return 0.5f * (max + min);
        }

        float Width() const
        {
            return max.x - min.x;
        }

        float Height() const
        {
            return max.y - min.y;
        }

        bool Empty() const
        {
            return min.y == INFINITY;
        }
    };

    struct ImRoundRect
    {
        Vec4 centerColor;
        Vec4 borderColor;

        Vec2 centerPos;
        Vec2 halfExtent;

        f32 cornerRadius;
        f32 borderWidth;

        Vec4 texTint;
        ImTextureID texIndex;
        Vec2 texCenterPos;
        Vec2 texHalfExtent;
    };

    enum class ImDrawType
    {
        RoundRect,
    };

    struct ImDrawCommand
    {
        ImDrawType type;
        u32 first;
        u32 count;
    };

// -----------------------------------------------------------------------------

    struct ImGlyph
    {
        Texture* texture;
        ImTextureID index;
        f32 width;
        f32 height;
        f32 advance;
        Vec2 offset;
    };

    struct ImFont
    {
        std::vector<ImGlyph> glyphs;
    };

// -----------------------------------------------------------------------------

    struct ImDraw2D
    {
        struct PushConstants
        {
            alignas(8) Vec2 invHalfExtent;
            alignas(8) Vec2 centerPos;
            u64 rectInstancesVA;
        };

        static constexpr VkPushConstantRange PushConstantsRange {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .size = sizeof(PushConstants),
        };

        static constexpr u32 MaxPrimitives = 65'536;

    public:
        Context* context = {};

        Sampler* defaultSampler = {};

        PipelineLayout* pipelineLayout = {};

        DescriptorLayout*     descriptorSetLayout = {};
        Buffer*                  descriptorBuffer = {};
        u32                       nextTextureSlot = 0;
        std::vector<u32>      textureSlotFreelist = {};

        Shader* rectVertShader = {};
        Shader* rectFragShader = {};
        Buffer*     rectBuffer = {};
        u32          rectIndex = 0;

        ImBounds2D bounds;

        std::vector<ImDrawCommand> drawCommands;

    public:
        static ImDraw2D* Create(Context* context);
        static void Destroy(ImDraw2D* imDraw);

        ImTextureID RegisterTexture(Texture* texture, Sampler* sampler);
        void UnregisterTexture(ImTextureID textureSlot);

        ImFont* LoadFont(const char* file, f32 size, CommandPool* cmdPool, ResourceTracker* tracker, Fence* fence, Queue* queue);
        void DestroyFont(ImFont* font);

        void Reset();
        void DrawRect(const ImRoundRect& rect);
        void DrawString(std::string_view str, Vec2 pos, ImFont* font);

        ImBounds2D MeasureString(std::string_view str, ImFont* font);

        void Record(CommandList* commandList);
    };
}