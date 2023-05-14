#include "Nova_Core.hpp"

namespace nova
{
    template<class T>
    T AlignUpPower2(T v, u64 align) {
        return T((u64(v) + (align - 1)) &~ (align - 1));
    }

    inline
    u32 RoundUpPower2(u32 v) {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;

        return v;
    }

    inline
    glm::quat QuatFromVec4(glm::vec4 v)
    {
        glm::quat q;
        q.x = v.x;
        q.y = v.y;
        q.z = v.z;
        q.w = v.w;
        return q;
    }

    inline
    glm::vec4 QuatToVec4(glm::quat q)
    {
        return { q.x, q.y, q.z, q.w };
    }
}