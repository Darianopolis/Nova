#include "nova_Math.hpp"

namespace nova
{
    Trs& Trs::TranslateWorld(Vec3 delta) noexcept
    {
        translation += delta;
        return *this;
    }

    Trs& Trs::TranslateLocal(Vec3 delta) noexcept
    {
        translation += rotation * (delta * scale);
        return *this;
    }

    Trs& Trs::RotateWorld(Quat delta) noexcept
    {
        rotation = glm::normalize(delta * rotation);
        translation = delta * translation;
        return *this;
    }

    Trs& Trs::RotateLocal(Quat delta)
    {
        rotation = glm::normalize(rotation * delta);
        return *this;
    }

    Trs& Trs::ScaleWorld(glm::vec3 delta) noexcept
    {
        scale *= delta;
        translation *= delta;
        return *this;
    }

    Trs& Trs::ScaleLocal(glm::vec3 delta) noexcept
    {
        scale *= delta;
        // translation *= delta; // TODO: Should we apply to translate here?
        return *this;
    }

    Mat4 Trs::GetMatrix() const noexcept
    {
        f32 rx = rotation.x, ry = rotation.y, rz = rotation.z, rw = rotation.w;
        f32 qxx = rx * rx, qyy = ry * ry, qzz = rz * rz;
        f32 qxz = rx * rz, qxy = rx * ry, qyz = ry * rz;
        f32 qwx = rw * rx, qwy = rw * ry, qwz = rw * rz;

        return {
            (1.f - 2.f * (qyy + qzz)) * scale.x,        2.f * (qxy + qwz)  * scale.x,        2.f * (qxz - qwy)  * scale.x, 0.f,
                    2.f * (qxy - qwz)  * scale.y, (1.f - 2.f * (qxx + qzz)) * scale.y,        2.f * (qyz + qwx)  * scale.y, 0.f,
                    2.f * (qxz + qwy)  * scale.z,        2.f * (qyz - qwx)  * scale.z, (1.f - 2.f * (qxx + qyy)) * scale.z, 0.f,
            translation.x, translation.y, translation.z, 1.f
        };
    }

    Mat4 Trs::GetInverseMatrix() const noexcept
    {
        // Unrolled and simplified version of
        //
        // auto t = glm::translate(Mat4(1.f), translation);
        // auto r = glm::mat4_cast(rotation);
        // auto s = glm::scale(Mat4(1.f), scale);
        // return glm::affineInverse(t * r * s);

        f32 rx = rotation.x, ry = rotation.y, rz = rotation.z, rw = rotation.w;
        f32 qxx = rx * rx, qyy = ry * ry, qzz = rz * rz;
        f32 qxz = rx * rz, qxy = rx * ry, qyz = ry * rz;
        f32 qwx = rw * rx, qwy = rw * ry, qwz = rw * rz;

        Vec3 r0 { (1.0f - 2.0f * (qyy + qzz)), (2.0f * (qxy - qwz)), (2.0f * (qxz + qwy)) };
        Vec3 r1 { (2.0f * (qxy + qwz)), (1.0f - 2.0f * (qxx + qzz)), (2.0f * (qyz - qwx)) };
        Vec3 r2 { (2.0f * (qxz - qwy)), (2.0f * (qyz + qwx)), (1.0f - 2.0f * (qxx + qyy)) };

        f32 sx = 1.f / scale.x, sy = 1.f / scale.y, sz = 1.f / scale.z;

        f32 dx = sx * (r0.x * translation.x + r1.x * translation.y + r2.x * translation.z);
        f32 dy = sy * (r0.y * translation.x + r1.y * translation.y + r2.y * translation.z);
        f32 dz = sz * (r0.z * translation.x + r1.z * translation.y + r2.z * translation.z);

        return {
            { r0.x * sx, r0.y * sy, r0.z * sz, 0.f },
            { r1.x * sx, r1.y * sy, r1.z * sz, 0.f },
            { r2.x * sx, r2.y * sy, r2.z * sz, 0.f },
            { -dx, -dy, -dz, 1.f }
        };
    }

    Trs operator*(const Trs& lhs, const Trs& rhs)
    {
        return {
            .translation = lhs.translation + lhs.scale.x * (lhs.rotation * rhs.translation),
            .rotation = lhs.rotation * rhs.rotation,
            .scale = lhs.scale * rhs.scale,
        };
    }

    Trs Trs::FromAffineTransform(Mat4 _matrix)
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
        for (u32 i = 0; i < 4; ++i) {
            trs.rotation[i] = f32(std::sqrt(f64(trs.rotation[i])) * 0.5f);
        }
        trs.rotation.x = std::copysignf(trs.rotation.x, matrix[6] - matrix[9]);
        trs.rotation.y = std::copysignf(trs.rotation.y, matrix[8] - matrix[2]);
        trs.rotation.z = std::copysignf(trs.rotation.z, matrix[1] - matrix[4]);

        return trs;
    }
}