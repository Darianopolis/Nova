#include <nova/rhi/slang/nova_Slang.slang>

struct Vertex
{
    float3 position;
    float3    color;
};

struct PushConstants
{
    Vertex *vertices;
    float3    offset;
};

#ifndef __cplusplus

[vk::push_constant] PushConstants pc;

struct VertexOutput
{
    float4 position : SV_Position;
    float3    color : COLOR;
};

[shader("vertex")]
VertexOutput VertexShader(uint id : SV_VertexID)
{
    Vertex v = pc.vertices[id];
    VertexOutput out = {};
    out.position = float4(v.position + pc.offset, 1);
    out.color = v.color;
    return out;
}

[shader("fragment")]
float4 FragmentShader(float3 color : COLOR)
{
    return float4(color, 1);
}

#endif