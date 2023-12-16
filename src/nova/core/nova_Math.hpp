#pragma once

#include "nova_Core.hpp"
#include "nova_Hash.hpp"

// -----------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable: 4201)

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/compatibility.hpp>

#pragma warning(pop)

// -----------------------------------------------------------------------------

namespace nova
{
    namespace types
    {
        using Vec2  = glm::vec2;
        using Vec2I = glm::ivec2;
        using Vec2U = glm::uvec2;

        using Vec3  = glm::vec3;
        using Vec3I = glm::ivec3;
        using Vec3U = glm::uvec3;

        using Vec4  = glm::vec4;
        using Vec4I = glm::ivec4;
        using Vec4U = glm::uvec4;

        using Quat = glm::quat;

        using Mat3 = glm::mat3;
        using Mat4 = glm::mat4;

        struct Rect2I
        {
            Vec2I offset, extent;
        };

        struct Bounds2
        {
            Vec2 min {  INFINITY,  INFINITY };
            Vec2 max { -INFINITY, -INFINITY };

            void Expand(const Bounds2& other) noexcept
            {
                min.x = std::min(min.x, other.min.x);
                min.y = std::min(min.y, other.min.y);
                max.x = std::max(max.x, other.max.x);
                max.y = std::max(max.y, other.max.y);
            }

            Vec2 Size()   const noexcept { return max - min; }
            Vec2 Center() const noexcept { return 0.5f * (max + min); }

            float Width()  const noexcept { return max.x - min.x; }
            float Height() const noexcept { return max.y - min.y; }

            bool Empty() const noexcept { return min.y == INFINITY; }
        };

        struct Trs
        {
            Vec3 translation = Vec3(0.f);
            Quat    rotation = Vec3(0.f);
            Vec3       scale = Vec3(1.f);

            Trs& TranslateWorld(Vec3 delta) noexcept;
            Trs& TranslateLocal(Vec3 delta) noexcept;

            Trs& RotateWorld(Quat delta) noexcept;
            Trs& RotateLocal(Quat delta);

            Trs& ScaleWorld(glm::vec3 delta) noexcept;
            Trs& ScaleLocal(glm::vec3 delta) noexcept;

            Mat4 GetMatrix() const noexcept;
            Mat4 GetInverseMatrix() const noexcept;

            static
            Trs FromAffineTransform(Mat4 matrix);

            friend
            Trs operator*(const Trs& lhs, const Trs& rhs);
        };

        struct Rect2D
        {
            Vec2I offset = {};
            Vec2U extent = {};
        };
    }
}

NOVA_MEMORY_HASH(nova::Vec2)
NOVA_MEMORY_HASH(nova::Vec2I)
NOVA_MEMORY_HASH(nova::Vec2U)
NOVA_MEMORY_HASH(nova::Vec3)
NOVA_MEMORY_HASH(nova::Vec3I)
NOVA_MEMORY_HASH(nova::Vec3U)
NOVA_MEMORY_HASH(nova::Vec4)
NOVA_MEMORY_HASH(nova::Vec4I)
NOVA_MEMORY_HASH(nova::Vec4U)
NOVA_MEMORY_HASH(nova::Quat)
NOVA_MEMORY_HASH(nova::Mat3)
NOVA_MEMORY_HASH(nova::Mat4)
NOVA_MEMORY_HASH(nova::Rect2I)
NOVA_MEMORY_HASH(nova::Trs)
NOVA_MEMORY_HASH(nova::Rect2D)

namespace nova
{

    template<typename T>
    constexpr
    T AlignUpPower2(T v, u64 align) noexcept
    {
        return T((u64(v) + (align - 1)) &~ (align - 1));
    }

    template<typename T>
    constexpr
    T AlignDownPower2(T v, u64 align) noexcept
    {
        return T(u64(v) &~ (align - 1));
    }

    template<typename T>
    requires std::is_pointer_v<T>
    constexpr
    T ByteOffsetPointer(T ptr, intptr_t offset) noexcept
    {
        return T(u64(ptr) + offset);
    }

    template<typename FieldT, typename ObjectT>
    constexpr
    FieldT& GetFieldAtByteOffset(const ObjectT& object, uintptr_t offset) noexcept
    {
        return *reinterpret_cast<FieldT*>(reinterpret_cast<uintptr_t>(&object) + offset);
    }

    template<typename ...T>
    constexpr
    usz SizeOf(usz count = 1) noexcept
    {
        return count * (... + (sizeof(T)));
    }

    inline constexpr
    u32 RoundUpPower2(u32 v) noexcept
    {
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
    glm::quat QuatFromVec4(glm::vec4 v) noexcept
    {
        glm::quat q;
        q.x = v.x;
        q.y = v.y;
        q.z = v.z;
        q.w = v.w;
        return q;
    }

    inline
    glm::vec4 QuatToVec4(glm::quat q) noexcept
    {
        return { q.x, q.y, q.z, q.w };
    }
}