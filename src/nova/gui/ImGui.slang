#include <nova/gpu/slang/Shared.slang>

#ifdef __cplusplus
namespace {
#endif

struct PushConstants
{
    ImDrawVert           *vertices;
    float2                   scale;
    float2                  offset;
    ImageSamplerDescriptor texture;
};

#ifdef __cplusplus
}
#else

struct ImDrawVert
{
    float2   pos;
    float2    uv;
    RGBA32 color;
};

[vk::push_constant] PushConstants pc;

struct VertexOutput
{
    float4   pos : SV_Position;
    float2    uv;
    float4 color;
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
float4 Fragment(in VertexOutput vout)
{
    return pc.texture.Sample(vout.uv) * vout.color;
}

#endif
