#include <nova/gpu/slang/Shared.slang>

static const float2 Positions[3] = { { -0.6, 0.6 }, { 0.6, 0.6 }, { 0, -0.6 } };
static const float3 Colors[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };

struct VertexOutput
{
    float4 position : SV_Position;
    float3    color;
};

[shader("vertex")]
VertexOutput Vertex(uint id : SV_VertexID)
{
    VertexOutput out = {};
    out.position = float4(Positions[id], 0, 1);
    out.color = Colors[id];
    return out;
}

[shader("fragment")]
float4 Fragment(in VertexOutput vout)
{
    return float4(vout.color, 1);
}
