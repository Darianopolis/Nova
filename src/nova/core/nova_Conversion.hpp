#pragma once

#include "nova_Math.hpp"
#include "nova_Debug.hpp"

namespace nova
{
    template<class Target, class Source>
    Target Cast(Source source, bool& test_pos, bool& pos_overflow, bool& test_neg, bool& neg_overflow)
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
                    std::cout << std::format("  max = {}, mantissa bits = {}, round_to {}, res = {}\n", intmax, mantissa, round_to, max);
                }
            }
            test_pos = true;
            pos_overflow = source > max;

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
                    std::cout << std::format("  min = {}, mantissa bits = {}, round_to {}, res = {}\n", intmin, mantissa, round_to, min);
                }
            }
            test_neg = true;
            neg_overflow = source < min;

            if (source < min) {
                NOVA_THROW("Negative Overflow");
            }
        }

        return static_cast<Target>(source);
    }

    template<class Source, class Target>
    void TestCast(Source value, bool test_pos, bool pos_overflow, bool test_neg, bool neg_overflow)
    {
        bool did_test_pos = false;
        bool did_test_neg = false;

        bool did_pos_overflow = false;
        bool did_neg_overflow = false;

        auto res = Cast<Target>(value, did_test_pos, did_pos_overflow, did_test_neg, did_neg_overflow);

        auto name = std::format("{}({}) -> {} Pos[{}/{}] Neg[{}/{}]",
            typeid(Source).name(), value,
            typeid(Target).name(),
            test_pos, pos_overflow, test_neg, neg_overflow);

        std::cout << std::format("Converted[{}]: {} -> {}\n", name, value, res);

        if (test_pos != did_test_pos) {
            std::cout << std::format("Failed[{}] Expected {}test for positive overflow\n", name, test_pos ? "" : "no ");
        } else if (pos_overflow != did_pos_overflow) {
            std::cout << std::format("Failed[{}] Expected pos overflow = {}, got = {}\n", name, pos_overflow, did_pos_overflow);
        }

        if (test_neg != did_test_neg) {
            std::cout << std::format("Failed[{}] Expected {}test for negative overflow\n", name, test_neg ? "" : "no ");
        } else if (neg_overflow != did_neg_overflow) {
            std::cout << std::format("Failed[{}] Expected neg overflow = {}, got = {}\n", name, neg_overflow, did_neg_overflow);
        }
    }

    void TestCastCases()
    {
        nova::TestCast<u32, u32>(0u,       false, false, false, false);
        nova::TestCast<u64, u32>(1ull<<20, true, false, false, false);
        nova::TestCast<u64, u32>(1ull<<40, true, true, false, false);

        nova::TestCast<i32, u32>( 0,     false, false, true, false);
        nova::TestCast<i32, u32>(-1,     false, false, true, true);
        nova::TestCast<i32, u16>( 65536, true, true, true, false);

        nova::TestCast<float, u32>(0.f, true, false, true, false);
        nova::TestCast<float, u32>(-1.f, true, false, true, true);
        nova::TestCast<float, u32>( 4294967040.f, true, false, true, false);
        nova::TestCast<float, u32>( 4294967296.f, true, true, true, false);
        nova::TestCast<float, i32>(-2147483648.f, true, false, true, false);
        nova::TestCast<float, i32>(-2147483904.f, true, false, true, true);
    }
}