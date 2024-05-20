#pragma once

#include "nova_Math.hpp"
#include "nova_Debug.hpp"

namespace nova
{
    template<typename Target, typename Source>
    Target Cast(Source source)
    {
        /*
            unsigned -> unsigned (max iff ssize >  tsize)
            unsigned -> signed   (max iff ssize >= tsize)
            unsigned -> float

            signed -> unsigned   (>0 && max iff ssize >= tsize)
            signed -> signed     (min/max iff ssize > tsize)
            signed -> float

            float -> unsigned    (>0 && max)
            float -> signed      (min/max)
            float -> float       (min/max iff ssize > tsize)
        */

        if constexpr (
               (std::is_unsigned_v<Source> && std::is_unsigned_v<Target> && sizeof(Source) >  sizeof(Target))
            || (std::is_unsigned_v<Source> && std::is_signed_v<Target>   && sizeof(Source) >= sizeof(Target))

            || (std::is_signed_v<Source>   && std::is_unsigned_v<Target> && sizeof(Source) >  sizeof(Target))
            || (std::is_signed_v<Source>   && std::is_signed_v<Target>   && sizeof(Source) >= sizeof(Target))

            || (std::is_floating_point_v<Source> && (!std::is_floating_point_v<Target> || sizeof(Source) > sizeof(Target)))
        ) {
            Source max = Source(std::numeric_limits<Target>::max());
            if constexpr (!std::is_floating_point_v<Target>) {
                constexpr u32 mantissa = std::is_same_v<Source, float> ? 24 : 53;
                if constexpr (mantissa < sizeof(Target) * CHAR_BIT) {
                    Target intmax = std::numeric_limits<Target>::max();
                    constexpr Target round_to = 1 << (sizeof(Target) * CHAR_BIT - mantissa);
                    max = AlignDownPower2(intmax, round_to);
                }
            }

            if (source > max) {
                NOVA_THROW("Positive Overflow");
            }
        }

        if constexpr (
               (std::is_signed_v<Source> && std::is_unsigned_v<Target>)
            || (std::is_signed_v<Source> && std::is_signed_v<Target> && sizeof(Source) >  sizeof(Target))
            || (std::is_floating_point_v<Source> && (!std::is_floating_point_v<Target> || sizeof(Source) > sizeof(Target)))
        ) {
            Source min = Source(std::numeric_limits<Target>::lowest());
            if constexpr (std::is_signed_v<Target>) {
                constexpr u32 mantissa = std::is_same_v<Source, float> ? 24 : 53;
                if constexpr (mantissa < sizeof(Target) * CHAR_BIT) {
                    Target intmin = std::numeric_limits<Target>::lowest();
                    constexpr Target round_to = 1 << (sizeof(Target) * CHAR_BIT - mantissa);
                    min = AlignUpPower2(intmin, round_to);
                }
            }

            if (source < min) {
                NOVA_THROW("Negative Overflow");
            }
        }

        return static_cast<Target>(source);
    }
}