#pragma once

#include <nova/rhi/nova_RHI.hpp>

namespace nova
{
    struct ImRoundRect
    {
        Vec4 centerColor;
        Vec4 borderColor;

        Vec2 centerPos;
        Vec2 halfExtent;

        f32 cornerRadius;
        f32 borderWidth;

        Vec4 texTint;
        u32 texIndex;
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

        VkSampler defaultSampler = {};
        Image*      defaultImage = {};

        VkPipelineLayout pipelineLayout = {};

        VkDescriptorSetLayout descriptorSetLayout = {};
        Buffer*                  descriptorBuffer = {};
        u32                          textureIndex = 0;

        Shader* rectVertShader = {};
        Shader* rectFragShader = {};
        Buffer*     rectBuffer = {};
        u32          rectIndex = 0;

        Vec2 minBounds, maxBounds;

        std::vector<ImDrawCommand> drawCommands;

    public:
        static ImDraw2D* Create(Context* context);
        static void Destroy(ImDraw2D* imDraw);

        void Reset();
        void DrawRect(const ImRoundRect& rect);

        void Record(CommandList* commandList);
    };
}