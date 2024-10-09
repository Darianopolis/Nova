#include <nova/gpu/slang/Shared.slang>

#ifdef __cplusplus
namespace {
#endif

struct PushConstants {
    ImageSamplerDescriptor source;
    ImageDescriptor target;
    float2 size;
    int horizontal;
    int blur_radius;
    const float *kernel;
};

#ifdef __cplusplus
}
#else

[vk::push_constant] PushConstants pc;

[shader("compute")]
[numthreads(16, 16, 1)]
void Compute(uint2 id : SV_DispatchThreadID) {
    float2 uv = float2(id) / pc.size;

    int blur_size = pc.blur_radius;

    float4 source = float4(0, 0, 0, 0);
    for (int i = -blur_size; i <= blur_size; i++) {
        float weight = pc.kernel[i + blur_size];
        if (pc.horizontal == 1) {
            source += pc.source.SampleLod(uv + float2(i + 0.5, 0 + 0.5) / pc.size, 0);
        } else {
            source += pc.source.SampleLod(uv + float2(0 + 0.5, i + 0.5) / pc.size, 0);
        }
    }
    source /= (blur_size * 2 + 1);

    // float4 source = pc.source.SampleLod(uv, 0);
    float4 dest = float4(1, 0, 1, 1);
    float3 color = lerp(dest.rgb, source.rgb, source.a);
    pc.target.Store(id, float4(color, 1));
}

#endif
