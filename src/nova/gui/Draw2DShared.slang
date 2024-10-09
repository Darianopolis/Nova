#pragma once

#include <nova/gpu/slang/Shared.slang>

namespace nova::draw
{
    struct Rectangle
    {
        RGBA32 center_color;
        RGBA32 border_color;

        float2 center_pos;
        float2 half_extent;
        float rotation;

        float corner_radius;
        float border_width;

        ImageSamplerDescriptor tex_handle;
        RGBA32 tex_tint;
        float2 tex_center_pos;
        float2 tex_half_extent;
    };
}
