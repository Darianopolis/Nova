#include "nova_ImDraw2D.hpp"

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
    vec2(-1, -1),
    vec2(-1,  1),
    vec2( 1, -1),

    vec2(-1,  1),
    vec2( 1,  1),
    vec2( 1, -1)
);

layout(location = 0) out vec2 outTex;
layout(location = 1) out uint outInstanceID;

void main()
{
    ImRoundRect box = ImRoundRectRef(pc.rectInstancesVA).data[gl_InstanceIndex];
    vec2 delta = deltas[gl_VertexIndex % 6];
    outTex = delta * box.halfExtent;
    outInstanceID = gl_InstanceIndex;
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

    vec4 sampled = box.texTint * texture(textures[nonuniformEXT(box.texIndex)], (inTex / box.halfExtent) * box.texHalfExtent + box.texCenterPos);
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

    {
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

        i32 w, h, c;
        auto data = stbi_load("assets/textures/statue.jpg", &w, &h, &c, STBI_rgb_alpha);
        NOVA_ON_SCOPE_EXIT(&) { stbi_image_free(data); };

        imDraw->defaultImage = context->CreateImage(
            Vec3(f32(w), f32(h), 0.f),
            ImageUsage::Sampled,
            Format::RGBA8U);

        context->CopyToImage(imDraw->defaultImage, data, w * h * 4);
        context->GenerateMips(imDraw->defaultImage);

        context->transferCmd->Transition(imDraw->defaultImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        context->graphics->Submit({context->transferCmd}, {}, {context->transferFence});
        context->transferFence->Wait();
        context->transferCommandPool->Reset();
        context->transferCmd = context->transferCommandPool->BeginPrimary(context->transferTracker);

        vkGetDescriptorEXT(context->device,
            Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .data = {
                    .pCombinedImageSampler = Temp(VkDescriptorImageInfo {
                        .sampler = imDraw->defaultSampler,
                        .imageView = imDraw->defaultImage->view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    }),
                },
            }),
            context->descriptorSizes.combinedImageSamplerDescriptorSize,
            imDraw->descriptorBuffer->mapped);
    }

// -----------------------------------------------------------------------------

        return imDraw;
    }

    void ImDraw2D::Destroy(ImDraw2D* imDraw)
    {
        imDraw->context->Destroy(imDraw->rectBuffer, imDraw->rectVertShader, imDraw->rectFragShader);
        vkDestroyPipelineLayout(imDraw->context->device, imDraw->pipelineLayout, imDraw->context->pAlloc);

        delete imDraw;
    }

    void ImDraw2D::Reset()
    {
        rectIndex = 0;
        textureIndex = 0;

        minBounds = Vec2(INFINITY, INFINITY);
        maxBounds = Vec2(-INFINITY, -INFINITY);

        drawCommands.clear();
    }

    void ImDraw2D::DrawRect(const ImRoundRect& rect)
    {
        ImDrawCommand& cmd = (drawCommands.size() && drawCommands.back().type == ImDrawType::RoundRect)
            ? drawCommands.back()
            : drawCommands.emplace_back(ImDrawType::RoundRect, rectIndex, 0);

        rectBuffer->Get<ImRoundRect>(cmd.first + cmd.count) = rect;

        minBounds.x = std::min(minBounds.x, rect.centerPos.x - rect.halfExtent.x);
        minBounds.y = std::min(minBounds.y, rect.centerPos.y - rect.halfExtent.y);
        maxBounds.x = std::max(maxBounds.x, rect.centerPos.x + rect.halfExtent.x);
        maxBounds.y = std::max(maxBounds.y, rect.centerPos.y + rect.halfExtent.y);

        cmd.count++;
    }

    void ImDraw2D::Record(CommandList* cmd)
    {
        cmd->SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        cmd->PushConstants(pipelineLayout,
            ShaderStage::Vertex | ShaderStage::Fragment,
            PushConstantsRange.offset, PushConstantsRange.size,
            Temp(PushConstants {
                .invHalfExtent = 2.f / (maxBounds - minBounds),
                .centerPos = (minBounds + maxBounds) / 2.f,
                .rectInstancesVA = rectBuffer->address,
            }));

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
                cmd->Draw(6, command.count, 0, command.first);
            }
        }
    }
}