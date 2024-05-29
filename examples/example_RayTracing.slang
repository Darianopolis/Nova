#include <nova/rhi/slang/nova_Slang.slang>

struct PushConstants
{
    AccelerationStructureHandle tlas;
    ImageDescriptor           target;
};

#ifndef __cplusplus

struct Payload
{
    float3          color;
    bool allow_reflection;
    bool           missed;
};

[vk::push_constant] PushConstants pc;

static const float3 Camera    = float3(0, 1.5, -7);
static const float3 Light     = float3(0,  200, 0);
static const float3 SkyTop    = float3(0.24, 0.44, 0.72);
static const float3 SkyBottom = float3(0.75, 0.86, 0.93);

[shader("raygeneration")]
void RayGeneration()
{
    uint2  idx = DispatchRaysIndex().xy;
    float2 size = DispatchRaysDimensions().xy;
    float2 uv = idx / size;
    float3 target = float3(
        (uv.x * 2 - 1) * 1.8 * (size.x / size.y),
        (1 - uv.y) * 4 - 2 + Camera.y,
        0);

    RayDesc ray;
    ray.Origin = Camera;
    ray.Direction = target - Camera;
    ray.TMin = 0.001;
    ray.TMax = 1000;

    Payload payload;
    payload.allow_reflection = true;
    payload.missed = false;

    TraceRay(pc.tlas.Get(), RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    pc.target.Store(idx, float4(payload.color, 1));
}

[shader("miss")]
void Miss(inout Payload payload)
{
    float slope = normalize(WorldRayDirection()).y;
    float t = saturate(slope * 5 + 0.5);
    payload.color = lerp(SkyBottom, SkyTop, t);
    payload.missed = true;
}

void HitCube(inout Payload payload, float2 uv);
void HitMirror(inout Payload payload, float2 uv);
void HitFloor(inout Payload payload, float2 uv);

[shader("closesthit")]
void ClosestHit(inout Payload payload, BuiltInTriangleIntersectionAttributes attrib)
{
    switch (InstanceID()) {
        break;case 0: HitCube(payload, attrib.barycentrics);
        break;case 1: HitMirror(payload, attrib.barycentrics);
        break;case 2: HitFloor(payload, attrib.barycentrics);
        break;default: payload.color = float3(1, 0, 1);
    }
}

void HitCube(inout Payload payload, float2 uv)
{
    uint tri = PrimitiveIndex();
    tri /= 2;
    float3 normal = (tri.xxx % 3 == uint3(0, 1, 2)) * (tri < 3 ? -1 : 1);
    float3 world_normal = normalize(mul(normal, (float3x3)ObjectToWorld4x3()));
    float3 color = abs(normal) / 3 + 0.5;
    if (uv.x < 0.03 || uv.y < 0.03)
        color = float3(0.25);
    color *= saturate(dot(world_normal, normalize(Light))) + 0.33;
    payload.color = color;
}

void HitMirror(inout Payload payload, float2 uv)
{
    if (!payload.allow_reflection) {
        payload.color = float3(0, 0, 0);
        return;
    }

    float3 pos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 normal = normalize(mul(float3(0, 1, 0), (float3x3)ObjectToWorld4x3()));
    float3 reflected = reflect(normalize(WorldRayDirection()), normal);

    RayDesc mirror_ray;
    mirror_ray.Origin = pos;
    mirror_ray.Direction = reflected;
    mirror_ray.TMin = 0.001;
    mirror_ray.TMax = 1000;

    payload.allow_reflection = false;
    TraceRay(pc.tlas.Get(), RAY_FLAG_NONE, 0xFF, 0, 0, 0, mirror_ray, payload);
}

void HitFloor(inout Payload payload, float2 uv)
{
    float3 pos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    bool2 pattern = frac(pos.xz) > 0.5;
    payload.color = pattern.x ^ pattern.y ? float3(0.6) : float3(0.4);

    RayDesc shadow_ray;
    shadow_ray.Origin = pos;
    shadow_ray.Direction = Light - pos;
    shadow_ray.TMin = 0.001;
    shadow_ray.TMax = 1;

    Payload shadow;
    shadow.allow_reflection = false;
    shadow.missed = false;
    TraceRay(pc.tlas.Get(), RAY_FLAG_NONE, 0xFF, 0, 0, 0, shadow_ray, shadow);

    if (!shadow.missed) {
        payload.color /= 2;
    }
}

#endif