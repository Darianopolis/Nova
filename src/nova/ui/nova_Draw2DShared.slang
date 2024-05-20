#pragma once

#include <nova/rhi/slang/nova_Slang.slang>

namespace nova::draw
{
struct Rectangle
{
    float4 center_color;
    float4 border_color;

    float2  center_pos;
    float2 half_extent;

    float corner_radius;
    float  border_width;

    float4                   tex_tint;
    ImageSamplerDescriptor tex_handle;
    float2             tex_center_pos;
    float2            tex_half_extent;
};
}