#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/rhi/nova_RHI.hpp>

// Colors

struct RGBA32
{
    uint32_t value;

    RGBA32() = default;

    RGBA32(const RGBA32&) = default;
    RGBA32& operator=(const RGBA32&) = default;

    RGBA32(glm::vec4 color)
        : value{glm::packUnorm4x8(color)}
    {}

    RGBA32(float r, float g, float b, float a)
        : value{glm::packUnorm4x8({r, g, b, a})}
    {}
};

// Descriptors

using nova::ImageSamplerDescriptor;
using nova::ImageDescriptor;
using AccelerationStructureHandle = nova::u64;

// Vectors

using float2 = nova::Vec2;
using float3 = nova::Vec3;
using float4 = nova::Vec4;

using uint = nova::u32;
using uint2 = nova::Vec2U;
using uint3 = nova::Vec3U;
using uint4 = nova::Vec4U;

using int2 = nova::Vec2I;
using int3 = nova::Vec3I;
using int4 = nova::Vec4I;

using bool2 = glm::bvec2;
using bool3 = glm::bvec3;
using bool4 = glm::bvec4;

// Matrices

using float2x2 = glm::mat2;
using float2x3 = glm::mat2x3;

using float3x2 = glm::mat3x2;
using float3x3 = glm::mat3;
using float3x4 = glm::mat3x4;

using float4x3 = glm::mat4x3;
using float4x4 = glm::mat4;