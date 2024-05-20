#include <nova/rhi/slang/nova_Slang.slang>

struct PushConstants
{
    ImDrawVert           *vertices;
    float2                   scale;
    float2                  offset;
    ImageSamplerDescriptor texture;
};

#ifndef __cplusplus

struct ImDrawVert
{
    float2   pos;
    float2    uv;
    RGBA32 color;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

struct VertexOutput
{
    float4   pos : SV_Position;
    float2    uv : UV;
    float4 color : COLOR;
};

[shader("vertex")]
VertexOutput Vertex(uint id : SV_VertexID)
{
    ImDrawVert v = pc.vertices[id];
    VertexOutput out;
    out.pos = float4((v.pos * pc.scale) + pc.offset, 0, 1);
    out.uv = v.uv;
    out.color = v.color.AsFloat4();
    return out;
}

[shader("fragment")]
float4 Fragment(float2 uv : UV, float4 color : COLOR)
{
    return pc.texture.Sample(uv) * color;
}

#endif