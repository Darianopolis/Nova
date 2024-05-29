#include <nova/rhi/slang/nova_Slang.slang>

struct PushConstants
{
    AccelerationStructureHandle tlas;
    ImageDescriptor           target;
    uint2                       size;
};

#ifndef __cplusplus

struct Payload
{
    float3 color;
};

[vk::push_constant] PushConstants pc;

[shader("raygeneration")]
void RayGeneration()
{
    uint2 idx = DispatchRaysIndex().xy;

    RayDesc ray;
    ray.Origin = float3(float2(idx), 1);
    ray.Direction = float3(0, 0, -1);
    ray.TMin = 0;
    ray.TMax = 2;

    Payload payload;
    payload.color = float3(0.1);

    TraceRay(pc.tlas.Get(), RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    pc.target.Store(idx, float4(payload.color, 1));
}

[shader("closesthit")]
void ClosestHit(inout Payload payload,
                BuiltInTriangleIntersectionAttributes attrib)
{
    float2 bary = attrib.barycentrics;
    payload.color = float3(1.0 - bary.x - bary.y, bary.x, bary.y);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void Compute(uint2 id : SV_DispatchThreadID)
{
    if (id.x >= pc.size.x || id.y >= pc.size.y) {
        return;
    }

    RayDesc ray;
    ray.Origin = float3(float2(id), 1);
    ray.Direction = float3(0, 0, -1);
    ray.TMin = 0;
    ray.TMax = 2;

    float3 color = float3(0.1);

    RayQuery<RAY_FLAG_NONE> query;
    query.TraceRayInline(pc.tlas.Get(), RAY_FLAG_NONE, 0xFF, ray);

    while (query.Proceed()) {
        if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE) {
            query.CommitNonOpaqueTriangleHit();
        }
    }

    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
        float2 bary = query.CommittedRayBarycentrics();
        color = float3(1.0 - bary.x - bary.y, bary.x, bary.y);float3(1.0 - bary.x - bary.y, bary.x, bary.y);
    }

    pc.target.Store(id, float4(color, 1.0));
}

#endif