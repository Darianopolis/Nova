#pragma once

#include "nova_Core.hpp"

namespace nova
{
    template<class T>
    constexpr
    T AlignUpPower2(T v, u64 align) noexcept {
        return T((u64(v) + (align - 1)) &~ (align - 1));
    }

    inline constexpr
    u32 RoundUpPower2(u32 v) noexcept {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;

        return v;
    }

    inline constexpr
    glm::quat QuatFromVec4(glm::vec4 v) noexcept
    {
        glm::quat q;
        q.x = v.x;
        q.y = v.y;
        q.z = v.z;
        q.w = v.w;
        return q;
    }

    inline constexpr
    glm::vec4 QuatToVec4(glm::quat q) noexcept
    {
        return { q.x, q.y, q.z, q.w };
    }
}