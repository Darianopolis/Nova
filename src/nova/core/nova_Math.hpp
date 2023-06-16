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

    inline
    TRS DecomposeAffineTransform(Mat4 _matrix)
    {
        TRS trs = {};
        auto* matrix = glm::value_ptr(_matrix);

        // Extract the translation.
        trs.translation = Vec3(matrix[12], matrix[13], matrix[14]);

        // Extract the scale. We calculate the euclidean length of the columns. We then
        // construct a vector with those lengths.
        auto s1 = std::sqrt(matrix[0] * matrix[0] + matrix[1] * matrix[1] +  matrix[2] *  matrix[2]);
        auto s2 = std::sqrt(matrix[4] * matrix[4] + matrix[5] * matrix[5] +  matrix[6] *  matrix[6]);
        auto s3 = std::sqrt(matrix[8] * matrix[8] + matrix[9] * matrix[9] + matrix[10] * matrix[10]);
        trs.scale = Vec3(s1, s2, s3);

        // Remove the scaling from the matrix, leaving only the rotation. matrix is now the
        // rotation matrix.
        matrix[0] /= s1; matrix[1] /= s1;  matrix[2] /= s1;
        matrix[4] /= s2; matrix[5] /= s2;  matrix[6] /= s2;
        matrix[8] /= s3; matrix[9] /= s3; matrix[10] /= s3;

        // Construct the quaternion. This algo is copied from here:
        // https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/christian.htm.
        // glm orders the components as w,x,y,z
        trs.rotation = {
            /* w = */ std::max(0.f, 1.f + matrix[0] + matrix[5] + matrix[10]),
            /* x = */ std::max(0.f, 1.f + matrix[0] - matrix[5] - matrix[10]),
            /* y = */ std::max(0.f, 1.f - matrix[0] + matrix[5] - matrix[10]),
            /* z = */ std::max(0.f, 1.f - matrix[0] - matrix[5] + matrix[10]),
        };
        for (u32 i = 0; i < 4; ++i)
            trs.rotation[i] = f32(std::sqrt(f64(trs.rotation[i])) / 2.f);
        trs.rotation.x = std::copysignf(trs.rotation.x, matrix[6] - matrix[9]);
        trs.rotation.y = std::copysignf(trs.rotation.y, matrix[8] - matrix[2]);
        trs.rotation.z = std::copysignf(trs.rotation.z, matrix[1] - matrix[4]);

        return trs;
    }
}