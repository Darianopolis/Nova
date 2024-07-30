#pragma once

#ifdef __cplusplus

#include <nova/rhi/slang/nova_Slang.hpp>

#else

// -----------------------------------------------------------------------------

[vk::binding(0, 0)] Texture2D             Image2D_Heap[];
[vk::binding(1, 0)] RWTexture2D<float4> RWImage2D_Heap[];
[vk::binding(2, 0)] SamplerState          Sampler_Heap[];

// -----------------------------------------------------------------------------

struct ImageHandle
{
    const static uint   ImageBitsMask   = 0xFFF;
    const static uint SamplerBitsOffset = 20;

    uint handle;

    __init(uint image)
    {
        this.handle = image;
    }

    __init(uint image, uint sampler)
    {
        this.handle = (image) | (sampler << SamplerBitsOffset);
    }

    float4 Sample(float2 uv)
    {
        return Image2D_Heap[NonUniformResourceIndex(handle & ImageBitsMask)]
            .Sample(Sampler_Heap[NonUniformResourceIndex(handle >> SamplerBitsOffset)], uv);
    }

    float4 SampleLod(float2 uv, uint lod)
    {
        return Image2D_Heap[NonUniformResourceIndex(handle & ImageBitsMask)]
            .SampleLevel(Sampler_Heap[NonUniformResourceIndex(handle >> SamplerBitsOffset)], uv, lod);
    }

    float4 Load(uint idx)
    {
        return RWImage2D_Heap[NonUniformResourceIndex(handle & ImageBitsMask)][idx];
    }

    float4 Store(uint2 idx, float4 value)
    {
        return RWImage2D_Heap[NonUniformResourceIndex(handle & ImageBitsMask)][idx] = value;
    }
};

// TODO: This stuff

// [[vk::binding(0, 0)]] __DynamicResource<__DynamicResourceKind.Sampler> g_SamplerHeap[];
// [[vk::binding(1, 0)]] __DynamicResource<__DynamicResourceKind.General> g_RWTextureHeap[];
// [[vk::binding(2, 0)]] __DynamicResource<__DynamicResourceKind.General> g_TextureHeap[];

// struct ImageHandle<T>
// {
//     uint32_t handle;

//     __init(uint32_t image) { handle = image; }
//     __init(uint32_t image, uint32_t sampler) { handle = image | (sampler << 20); }

//     __subscript(uint32_t idx)->T
//     {
//         get { return g_TextureHeap[NonUniformResourceIndex(handle & 0xfffff)].as<Texture2D<T>>()[idx]; }
//         set { g_RWTextureHeap[NonUniformResourceIndex(handle & 0xfffff)].as<RWTexture2D<T>>()[idx] = newValue; }
//     }

//     __subscript(uint2 idx)->T
//     {
//         get { return g_TextureHeap[NonUniformResourceIndex(handle & 0xfffff)].as<Texture2D<T>>()[idx]; }
//         set { g_RWTextureHeap[NonUniformResourceIndex(handle & 0xfffff)].as<RWTexture2D<T>>()[idx] = newValue; }
//     }

//     uint2 GetDimensions()
//     {
//         uint width, height;
//         g_TextureHeap[NonUniformResourceIndex(handle & 0xfffff)].as<Texture2D<T>>().GetDimensions(width, height);

//         return uint2(width, height);
//     }
// }

// __generic<T : IFloat>
//     extension ImageHandle<T>
// {
//     T Sample(float2 uv)
//     {
//         return g_TextureHeap[NonUniformResourceIndex(handle & 0xfffff)].as<Texture2D<T>>().Sample(g_SamplerHeap[NonUniformResourceIndex(handle >> 20)].as<SamplerState>(), uv);
//     }

//     T SampleLod(float2 uv, uint32_t lod)
//     {
//         return g_TextureHeap[NonUniformResourceIndex(handle & 0xfffff)].as<Texture2D<T>>().SampleLevel(g_SamplerHeap[NonUniformResourceIndex(handle >> 20)].as<SamplerState>(), uv, lod);
//     }
// }

typedef ImageHandle ImageSamplerDescriptor;
typedef ImageHandle ImageDescriptor;

// -----------------------------------------------------------------------------

struct AccelerationStructureHandle
{
    uint64_t handle;

    [vk::spirv_instruction(/* OpConvertUToAccelerationStructureKHR */ 4447, "SPV_KHR_ray_tracing")]
    private static RaytracingAccelerationStructure ConvertToAS(uint64_t handle);

    RaytracingAccelerationStructure Get()
    {
        return ConvertToAS(handle);
    }
};

// -----------------------------------------------------------------------------

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