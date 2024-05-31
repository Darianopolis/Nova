#include <nova/rhi/slang/nova_Slang.slang>

#ifdef __cplusplus
namespace {
#endif

struct PushConstants {
    ImageSamplerDescriptor source;
    ImageDescriptor        target;
    float2                   size;
};

#ifdef __cplusplus
}
#else

[vk::push_constant] PushConstants pc;

[shader("compute")]
[numthreads(16, 16, 1)]
void Compute(uint2 id : SV_DispatchThreadID) {
    float2 uv = float2(id) / pc.size;

    float4 source = pc.source.SampleLod(uv, 0);
    float4 dest = float4(1, 0, 1, 1);
    float3 color = lerp(dest.rgb, source.rgb, source.a);
    pc.target.Store(id, float4(color, 1));
}

#endif