#pragma once

#include "nova_Core.hpp"

namespace nova
{
    inline
    std::string DurationToString(std::chrono::duration<double, std::nano> dur)
    {
        f64 nanos = dur.count();

        if (nanos > 1e9)
        {
            f64 seconds = nanos / 1e9;
            u32 decimals = 2 - u32(std::log10(seconds));
            return std::format("{:.{}f}s",seconds, decimals);
        }
        if (nanos > 1e6)
        {
            f64 millis = nanos / 1e6;
            u32 decimals = 2 - u32(std::log10(millis));
            return std::format("{:.{}f}ms", millis, decimals);
        }
        if (nanos > 1e3)
        {
            f64 micros = nanos / 1e3;
            u32 decimals = 2 - u32(std::log10(micros));
            return std::format("{:.{}f}us", micros, decimals);
        }
        if (nanos > 0)
        {
            u32 decimals = 2 - u32(std::log10(nanos));
            return std::format("{:.{}f}ns", nanos, decimals);
        }

        return "0";
    }

    inline
    std::string ByteSizeToString(u64 bytes)
    {
        auto Gigabyte = 1ull << 30;
        auto Megabyte = 1ull << 20;
        auto Kilobyte = 1ull << 10;

        if (bytes > Gigabyte)
        {
            f64 gigabytes = bytes / f64(Gigabyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(gigabytes)));
            return std::format("{:.{}f}GB", gigabytes, decimals);
        }
        if (bytes > Megabyte)
        {
            f64 megabytes = bytes / f64(Megabyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(megabytes)));
            std::string str =  std::format("{:.{}f}MB", megabytes, decimals);
            return str;
        }
        if (bytes > Kilobyte)
        {
            f64 kilobytes = bytes / f64(Kilobyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(kilobytes)));
            return std::format("{:.{}f}KB", kilobytes, decimals);
        }
        if (bytes > 0)
        {
            u32 decimals = 2 - std::min(2u, u32(std::log10(bytes)));
            return std::format("{:.{}f}", f64(bytes), decimals);
        }

        return "0";
    }

    template<typename T>
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
    Trs DecomposeAffineTransform(Mat4 _matrix)
    {
        Trs trs = {};
        auto* matrix = glm::value_ptr(_matrix);

        // Extract the translation.
        trs.translation = Vec3(matrix[12], matrix[13], matrix[14]);

        // Extract the scale. We calculate the euclidean length of the columns. We then
        // construct a vector with those lengths.
        f32 s1 = std::sqrt(matrix[0] * matrix[0] + matrix[1] * matrix[1] +  matrix[2] *  matrix[2]);
        f32 s2 = std::sqrt(matrix[4] * matrix[4] + matrix[5] * matrix[5] +  matrix[6] *  matrix[6]);
        f32 s3 = std::sqrt(matrix[8] * matrix[8] + matrix[9] * matrix[9] + matrix[10] * matrix[10]);
        trs.scale = Vec3(s1, s2, s3);

        f32 is1 = 1.f / s1;
        f32 is2 = 1.f / s2;
        f32 is3 = 1.f / s3;

        // Remove the scaling from the matrix, leaving only the rotation. matrix is now the
        // rotation matrix.
        matrix[0] *= is1; matrix[1] *= is1;  matrix[2] *= is1;
        matrix[4] *= is2; matrix[5] *= is2;  matrix[6] *= is2;
        matrix[8] *= is3; matrix[9] *= is3; matrix[10] *= is3;

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
            trs.rotation[i] = f32(std::sqrt(f64(trs.rotation[i])) * 0.5f);
        trs.rotation.x = std::copysignf(trs.rotation.x, matrix[6] - matrix[9]);
        trs.rotation.y = std::copysignf(trs.rotation.y, matrix[8] - matrix[2]);
        trs.rotation.z = std::copysignf(trs.rotation.z, matrix[1] - matrix[4]);

        return trs;
    }
}