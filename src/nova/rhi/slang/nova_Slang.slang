#pragma once

#ifdef __cplusplus

#include <nova/rhi/slang/nova_Slang.hpp>

#else

[[vk::binding(0, 0)]] Texture2D             Image2D_Heap[];
[[vk::binding(1, 0)]] RWTexture2D<float4> RWImage2D_Heap[];
[[vk::binding(2, 0)]] SamplerState          Sampler_Heap[];

struct Image
{
    uint handle;

    __init(uint handle) { this.handle = handle; }

    __init(uint image, uint sampler) { this.handle = (image & 0xFFFFF) | ((sampler & 0xFFF) << 20); }

    float4 Sample(float2 uv)
    {
        return Image2D_Heap[NonUniformResourceIndex(handle & 0xFFFFF)].SampleLevel(Sampler_Heap[NonUniformResourceIndex(handle >> 20)], uv, 0);
    }

    float4 Load(uint idx)
    {
        return RWImage2D_Heap[NonUniformResourceIndex(handle & 0xFFFFF)][idx];
    }

    float4 Store(uint2 idx, float4 value)
    {
        return RWImage2D_Heap[NonUniformResourceIndex(handle & 0xFFFFF)][idx] = value;
    }
};

typedef Image ImageSamplerDescriptor;
typedef Image ImageDescriptor;

struct AccelerationStructureHandle
{
    uint64_t handle;

    [vk::spirv_instruction(/* OpConvertUToAccelerationStructureKHR */ 4447, "SPV_KHR_ray_tracing")]
    private static RaytracingAccelerationStructure ConvertToAS(uint64_t handle);

    RaytracingAccelerationStructure Get()
    {
        return ConvertToAS(handle);
    }
}

struct RGBA32
{
    uint value;

    float4 AsFloat4()
    {
        uint r =  value        & 0xFF;
        uint g = (value >>  8) & 0xFF;
        uint b = (value >> 16) & 0xFF;
        uint a = (value >> 24) & 0xFF;

        return float4(r, g, b, a) / 255.0;
    }
};

#endif