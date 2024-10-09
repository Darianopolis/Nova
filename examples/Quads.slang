#include <nova/gpu/slang/Shared.slang>

#ifdef __cplusplus
namespace {
#endif

struct Quad
{
    float2 position;
};

struct PushConstants
{
    const Quad* quads;
    float2  quad_size;
};

#ifdef __cplusplus
}
#else

[vk::push_constant] PushConstants pc;

struct VertexOutput
{
    float4 position : SV_Position;
    float2       uv;
};

static const float2 positions[] = {
    { 0, 1 }, { 1, 1 }, { 0, 0 },
    { 1, 1 }, { 1, 0 }, { 0, 0 },
};

VertexOutput Vertex(uint qid, uint lid)
{
    Quad q = pc.quads[qid];
    float2 local_pos = positions[lid];
    VertexOutput out;
    out.position = float4(q.position + (local_pos * pc.quad_size), 0, 1);
    out.uv = (q.position * 0.5) + 0.5;
    return out;
}

[shader("vertex")]
VertexOutput VertexBatched(uint id : SV_VertexID)
{
    return Vertex(id / 6, id % 6);
}

[shader("vertex")]
VertexOutput VertexInstanced(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    return Vertex(iid, vid);
}

[shader("fragment")]
float4 Fragment(in VertexOutput vout)
{
    return float4(vout.uv, 0, 1);
}

#endif
