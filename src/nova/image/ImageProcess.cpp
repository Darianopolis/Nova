#include "Image.hpp"

namespace nova
{
    f32 SRGB_ToNonLinear(f32 v)
    {
        return (v < 0.0031308f)
            ? v * 12.92f
            : (1.055f * std::pow(v, 1.f / 2.4f)) - 0.055f;
    }

    f32 SRGB_ToLinear(f32 v)
    {
        return (v < 0.4045f)
            ? v / 12.92f
            : std::pow((v + 0.055f) / 1.055f, 2.4f);
    }
}