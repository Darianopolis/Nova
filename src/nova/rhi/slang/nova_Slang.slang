#pragma once

#ifdef __cplusplus

#include <nova/rhi/slang/nova_Slang.hpp>

#else

[vk::binding(0, 0)] Texture2D             Image2D_Heap[];
[vk::binding(1, 0)] RWTexture2D<float4> RWImage2D_Heap[];
[vk::binding(2, 0)] SamplerState          Sampler_Heap[];

struct Image
{
    const static uint   ImageHandleMask =    0xFFFFF;
    const static uint SamplerHandleMask = 0xFFF;

    const static uint   ImageHandleMaskWidth = 20;
    const static uint SamplerHandleMaskWidth = 12;

    uint handle;

    __init(uint handle) { this.handle = handle; }

    __init(uint image, uint sampler) { this.handle = (image & ImageHandleMask) | ((sampler & SamplerHandleMask) << ImageHandleMaskWidth); }

    float4 Sample(float2 uv)
    {
        return Image2D_Heap[NonUniformResourceIndex(handle & ImageHandleMask)].Sample(Sampler_Heap[NonUniformResourceIndex(handle >> ImageHandleMaskWidth)], uv);
    }

    float4 SampleLod(float2 uv, uint lod)
    {
        return Image2D_Heap[NonUniformResourceIndex(handle & ImageHandleMask)].SampleLevel(Sampler_Heap[NonUniformResourceIndex(handle >> ImageHandleMaskWidth)], uv, lod);
    }

    float4 Load(uint idx)
    {
        return RWImage2D_Heap[NonUniformResourceIndex(handle & ImageHandleMask)][idx];
    }

    float4 Store(uint2 idx, float4 value)
    {
        return RWImage2D_Heap[NonUniformResourceIndex(handle & ImageHandleMask)][idx] = value;
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