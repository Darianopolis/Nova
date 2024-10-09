#include <nova/gpu/slang/Shared.slang>

#ifdef __cplusplus
namespace {
#endif

struct Vertex
{
    float3 position;
    float3    color;
};

struct PushConstants
{
    ImageSamplerDescriptor image;
    const Vertex*       vertices;
    float3 offset;
};

#ifdef __cplusplus
}
#else

[vk::push_constant] PushConstants pc;

struct VertexOutput
{
    float4 position : SV_Position;
    float3    color;
    float2       uv;
};

[shader("vertex")]
VertexOutput VertexShader(uint id : SV_VertexID)
{
    Vertex v = pc.vertices[id];
    VertexOutput out = {};
    out.position = float4(v.position + pc.offset, 1);
    out.color = v.color;
    out.uv = (out.position.xy * 0.5 / 0.6) + 0.5;
    return out;
}

[shader("fragment")]
float4 FragmentShader(in VertexOutput vout)
{
    return lerp(float4(vout.color, 1), pc.image.Sample(vout.uv), 0.5);
}

#endif
