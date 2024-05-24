#pragma once

// -----------------------------------------------------------------------------
//                         Standard Library Includes
// -----------------------------------------------------------------------------

#include <algorithm>
#include <any>
#include <array>
#include <chrono>
#include <climits>
#include <concepts>
#include <condition_variable>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <execution>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <future>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <ranges>
#include <semaphore>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <string>
#include <syncstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <regex>
#include <bitset>
#include <cstdlib>
#include <stacktrace>
#include <typeindex>

// -----------------------------------------------------------------------------
//                            Standard Namespaces
// -----------------------------------------------------------------------------

using namespace std::literals;

namespace nova
{
    namespace fs = std::filesystem;
    namespace chr = std::chrono;
}

// -----------------------------------------------------------------------------
//                                   GLM
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
//                                 mimalloc
// -----------------------------------------------------------------------------

#include <mimalloc.h>

// -----------------------------------------------------------------------------
//                                 simdutf
// -----------------------------------------------------------------------------

#include <simdutf.h>

// -----------------------------------------------------------------------------
//                           ankerl unordered_dense
// -----------------------------------------------------------------------------

#include <ankerl/unordered_dense.h>

// -----------------------------------------------------------------------------
//                                  fmtlib
// -----------------------------------------------------------------------------

#include <fmt/format.h>
#include <fmt/ostream.h>

// -----------------------------------------------------------------------------
//                            Ankerl Hash Helpers
// -----------------------------------------------------------------------------

#define NOVA_MEMORY_EQUALITY_MEMBER(type)                                      \
    bool operator==(const type& other) const                                   \
    {                                                                          \
        return std::memcmp(this, &other, sizeof(type)) == 0;                   \
    }

#define NOVA_MEMORY_HASH(type)                                                 \
    template<>                                                                 \
    struct ankerl::unordered_dense::hash<type>                                 \
    {                                                                          \
        using is_avalanching = void;                                           \
        uint64_t operator()(const type& key) const noexcept {                  \
            return detail::wyhash::hash(&key, sizeof(key));                    \
        }                                                                      \
    };

// -----------------------------------------------------------------------------
//                        fmtlib standard formatters
// -----------------------------------------------------------------------------

template<typename ... DurationT>
struct fmt::formatter<std::chrono::duration<DurationT...>> : fmt::ostream_formatter {};

template<>
struct fmt::formatter<std::stacktrace> : fmt::ostream_formatter {};

// -----------------------------------------------------------------------------
//                            Nova Types namespace
// -----------------------------------------------------------------------------

namespace nova
{
    namespace types {}

    using namespace types;
}

// -----------------------------------------------------------------------------
//                            Core scalar types
// -----------------------------------------------------------------------------

namespace nova::types
{
    using u8  = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using i8  = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;

    using uc8 = unsigned char;
    using c8  = char;
    using c16 = wchar_t;
    using c32 = char32_t;

    using f32 = float;
    using f64 = double;

    using b8 = std::byte;

    using usz = std::size_t;
}

// -----------------------------------------------------------------------------
//                         Core vector/matrix types
// -----------------------------------------------------------------------------

namespace nova::types
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

namespace nova
{
    inline
    Quat QuatFromVec4(Vec4 v) noexcept
    {
        glm::quat q;
        q.x = v.x;
        q.y = v.y;
        q.z = v.z;
        q.w = v.w;
        return q;
    }

    inline
    Vec4 QuatToVec4(Quat q) noexcept
    {
        return { q.x, q.y, q.z, q.w };
    }
}

// -----------------------------------------------------------------------------
//                          Composite vector types
// -----------------------------------------------------------------------------

namespace nova::types
{
    struct Rect2I
    {
        Vec2I offset, extent;
    };

    struct Rect2D
    {
        Vec2I offset = {};
        Vec2U extent = {};
    };

    struct Bounds2F
    {
        Vec2 min {  INFINITY,  INFINITY };
        Vec2 max { -INFINITY, -INFINITY };

        void Expand(const Bounds2F& other) noexcept
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
}

NOVA_MEMORY_HASH(nova::Rect2I)
NOVA_MEMORY_HASH(nova::Rect2D)
NOVA_MEMORY_HASH(nova::Bounds2F)

// -----------------------------------------------------------------------------
//                     Translation Rotation Scale (TRS)
// -----------------------------------------------------------------------------

namespace nova::types
{
    struct Trs
    {
        Vec3 translation = Vec3(0.f);
        Quat    rotation = Vec3(0.f);
        Vec3       scale = Vec3(1.f);

        Trs& TranslateWorld(Vec3 delta) noexcept
        {
            translation += delta;
            return *this;
        }

        Trs& TranslateLocal(Vec3 delta) noexcept
        {
            translation += rotation * (delta * scale);
            return *this;
        }

        Trs& RotateWorld(Quat delta) noexcept
        {
            rotation = glm::normalize(delta * rotation);
            translation = delta * translation;
            return *this;
        }

        Trs& RotateLocal(Quat delta)
        {
            rotation = glm::normalize(rotation * delta);
            return *this;
        }

        Trs& ScaleWorld(glm::vec3 delta) noexcept
        {
            scale *= delta;
            translation *= delta;
            return *this;
        }

        Trs& ScaleLocal(glm::vec3 delta) noexcept
        {
            scale *= delta;
            // translation *= delta; // TODO: Should we apply to translate here?
            return *this;
        }

        Mat4 GetMatrix() const noexcept
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

        Mat4 GetInverseMatrix() const noexcept
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

        friend
        Trs operator*(const Trs& lhs, const Trs& rhs)
        {
            return {
                .translation = lhs.translation + lhs.scale.x * (lhs.rotation * rhs.translation),
                .rotation = lhs.rotation * rhs.rotation,
                .scale = lhs.scale * rhs.scale,
            };
        }

        Trs FromAffineTransform(Mat4 _matrix)
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
    };
}

NOVA_MEMORY_HASH(nova::Trs)

// -----------------------------------------------------------------------------
//                               Align Helpers
// -----------------------------------------------------------------------------

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
}

// -----------------------------------------------------------------------------
//                           Memory Access Utilities
// -----------------------------------------------------------------------------

namespace nova
{
    template<typename T>
    requires std::is_pointer_v<T>
    constexpr
    T ByteOffsetPointer(T ptr, intptr_t offset) noexcept
    {
        return T(uintptr_t(ptr) + offset);
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
}

// -----------------------------------------------------------------------------
//                            Functor overload set
// -----------------------------------------------------------------------------

namespace nova
{
    template<typename... Ts>
    struct Overloads : Ts... {
        using Ts::operator()...;
    };

    template<typename... Ts> Overloads(Ts...) -> Overloads<Ts...>;
}

// -----------------------------------------------------------------------------
//                       Pointer to temporary helper
// -----------------------------------------------------------------------------

namespace nova
{
    template<typename T>
    T* PtrTo(T&& v)
    {
        return &v;
    }
}

// -----------------------------------------------------------------------------

#define NOVA_CONCAT_INTERNAL(a, b) a##b
#define NOVA_CONCAT(a, b) NOVA_CONCAT_INTERNAL(a, b)
#define NOVA_UNIQUE_VAR() NOVA_CONCAT(nova_var, __COUNTER__)

// -----------------------------------------------------------------------------

#define NOVA_IGNORE(var) (void)var

// -----------------------------------------------------------------------------
//                        debug operator new overloads
// -----------------------------------------------------------------------------

inline
void* __cdecl operator new[](size_t size, const char* /* name */, int /* flags */, unsigned /* debug_flags */, const char* /* file */, int /* line */)
{
	return ::operator new(size);
}

inline
void* __cdecl operator new[](size_t size, size_t align, size_t /* ??? */, const char* /* name */, int /* flags */, unsigned /* debug_flags */, const char* /* file */, int /* line */)
{
    return ::operator new(size, std::align_val_t(align));
}

// -----------------------------------------------------------------------------
//                             Inline specifiers
// -----------------------------------------------------------------------------

#ifdef NOVA_PLATFORM_WINDOWS

#define NOVA_NO_INLINE __declspec(noinline)
#define NOVA_FORCE_INLINE __forceinline

#endif

// -----------------------------------------------------------------------------
//                                  Flags
// -----------------------------------------------------------------------------

#define NOVA_DECORATE_FLAG_ENUM(EnumType)                                      \
    inline constexpr EnumType operator|(EnumType l, EnumType r) {              \
        return EnumType(std::to_underlying(l) | std::to_underlying(r));        \
    }                                                                          \
    inline constexpr EnumType operator|=(EnumType& l, EnumType r) {            \
        return l = l | r;                                                      \
    }                                                                          \
    inline constexpr bool operator>=(EnumType l, EnumType r) {                 \
        return std::to_underlying(r)                                           \
            == (std::to_underlying(l) & std::to_underlying(r));                \
    }                                                                          \
    inline constexpr bool operator&(EnumType l, EnumType r) {                  \
        return static_cast<std::underlying_type_t<EnumType>>(0)                \
            != (std::to_underlying(l) & std::to_underlying(r));                \
    }

// -----------------------------------------------------------------------------
//                                 Guards
// -----------------------------------------------------------------------------

namespace nova::guards
{
    template<typename Fn>
    struct DoOnceGuard
    {
        DoOnceGuard(Fn&& fn)
        {
            fn();
        }
    };

    template<typename Fn>
    struct OnExitGuard
    {
        Fn fn;

        OnExitGuard(Fn&& _fn)
            : fn(std::move(_fn))
        {}

        ~OnExitGuard()
        {
            fn();
        }
    };

    template<typename Fn>
    class CleanupGuard
    {
        Fn fn;

    public:
        CleanupGuard(Fn fn)
            : fn(std::move(fn))
        {}

        ~CleanupGuard()
        {
            fn();
        }
    };

    template<typename Fn>
    class DeferGuard
    {
        Fn fn;
        i32 exceptions;

    public:
        DeferGuard(Fn fn)
            : fn(std::move(fn))
            , exceptions(std::uncaught_exceptions())
        {}

        ~DeferGuard()
        {
            fn(std::uncaught_exceptions() - exceptions);
        }
    };
}

#define NOVA_DO_ONCE(...) static ::nova::guards::DoOnceGuard NOVA_UNIQUE_VAR() = [__VA_ARGS__]
#define NOVA_ON_EXIT(...) static ::nova::guards::OnExitGuard NOVA_UNIQUE_VAR() = [__VA_ARGS__]
#define NOVA_DEFER(...)          ::nova::guards::DeferGuard NOVA_UNIQUE_VAR() = [__VA_ARGS__]([[maybe_unused]] ::nova::i32 exceptions)

// -----------------------------------------------------------------------------
//                                Allocation
// -----------------------------------------------------------------------------

namespace nova
{
    enum class AllocationType
    {
        Commit  = 1 << 0,
        Reserve = 1 << 1,
    };
    NOVA_DECORATE_FLAG_ENUM(AllocationType)

    enum class FreeType
    {
        Decommit = 1 << 0,
        Release  = 1 << 1,
    };
    NOVA_DECORATE_FLAG_ENUM(FreeType)

    void* AllocVirtual(AllocationType type, usz size);
    void FreeVirtual(FreeType type, void* ptr, usz size = 0);

    inline
    void* Alloc(usz size)
    {
        return mi_malloc(size);
    }

    inline
    void* Alloc(usz size, usz align)
    {
        return mi_malloc_aligned(size, align);
    }

    inline
    void Free(void* ptr)
    {
        mi_free(ptr);
    }
}

// -----------------------------------------------------------------------------
//                                  Span
// -----------------------------------------------------------------------------

namespace nova
{
    template<typename T>
    concept IsContiguousContainer = requires(T t)
    {
        { t.begin() } -> std::contiguous_iterator;
        { t.end() }   -> std::contiguous_iterator;
    };

    template<typename T>
    struct Span
    {
        std::span<const T> span;

    public:
        Span() = default;

        Span(const std::initializer_list<T>& init)
            : span(init.begin(), init.end())
        {}

        template<typename C>
        requires IsContiguousContainer<C>
        Span(C&& c)
            : span(c.begin(), c.end())
        {}

        Span(const T& v)
            : span(&v, 1ull)
        {}

        Span(const T* first, usz count) : span(first, count) {}

    public:
        const T& operator[](usz i) const noexcept { return span.begin()[i]; }

        Span(const Span& other) noexcept : span(other.span) {}
        Span& operator=(const Span& other) noexcept { span = other.span; return *this; }

        decltype(auto) begin() const noexcept { return span.data(); }
        decltype(auto) end()   const noexcept { return span.data() + span.size(); }

        const T* data() const noexcept { return span.data(); }
        usz size() const noexcept { return span.size(); }
        bool empty() const noexcept { return span.empty(); }
    };
}

// -----------------------------------------------------------------------------
//                        Type erased functor reference
// -----------------------------------------------------------------------------

namespace nova
{
    template<typename Ret, typename... Types>
    struct FuncBase {
        void* body;
        Ret(*fptr)(void*, Types...);

        Ret operator()(Types... args) {
            return fptr(body, std::move<Types>(args)...);
        }
    };

    template<typename Tx>
    struct GetFunctionImpl {};

    template<typename Ret, typename... Types>
    struct GetFunctionImpl<Ret(Types...)> { using type = FuncBase<Ret, Types...>; };

    template<typename Sig>
    struct FunctionRef : GetFunctionImpl<Sig>::type {
        template<typename Fn>
        FunctionRef(Fn&& fn)
            : GetFunctionImpl<Sig>::type(&fn,
                [](void*b, auto&&... args) -> auto {
                    return (*(Fn*)b)(std::forward<decltype(args)>(args)...);
                })
        {};
    };
}

// -----------------------------------------------------------------------------
//                          Type erased Byte-view
// -----------------------------------------------------------------------------

namespace nova
{
    struct RawByteView
    {
        const void* data;
        usz         size;

        template<typename T>
        RawByteView(const T& t)
            : data((const void*)&t), size(sizeof(T))
        {}

        RawByteView(const void* data, usz size)
            : data(data), size(size)
        {}
    };
}

// -----------------------------------------------------------------------------
//                            Type erased handle
// -----------------------------------------------------------------------------

namespace nova
{
    template<typename T>
    struct Handle
    {
        struct Impl;

    protected:
        Impl* impl = {};

    public:
        Handle() = default;
        Handle(Impl* _impl): impl(_impl) {};

        operator T() const noexcept { return T(impl); }
        T   Unwrap() const noexcept { return T(impl); }

        operator   bool() const noexcept { return impl; }
        auto operator->() const noexcept { return impl; }

        bool operator==(Handle other) const noexcept { return impl == other.impl; }
    };
}

// -----------------------------------------------------------------------------
//                              Index free list
// -----------------------------------------------------------------------------

namespace nova
{
    struct IndexFreeList
    {
        u32             next_index = 0;
        std::vector<u32> free_list;

    public:
        inline
        u32 Acquire()
        {
            if (free_list.empty())
                return next_index++;

            u32 index = free_list.back();
            free_list.pop_back();
            return index;
        }

        inline
        void Release(u32 index)
        {
            free_list.push_back(index);
        }
    };
}

// -----------------------------------------------------------------------------
//                               Hash Map/Set
// -----------------------------------------------------------------------------

namespace nova
{
    template<typename K, typename V>
    using HashMap = ankerl::unordered_dense::map<K, V>;

    template<typename E>
    using HashSet = ankerl::unordered_dense::set<E>;
}

// -----------------------------------------------------------------------------
//                         Standard Ranges Helpers
// -----------------------------------------------------------------------------

namespace nova
{
    inline constexpr const auto& Enumerate = std::ranges::views::enumerate;
}

// -----------------------------------------------------------------------------
//                                  Hashing
// -----------------------------------------------------------------------------

namespace nova::hash
{
    inline
    u64 Hash(const void* data, usz size)
    {
        return ankerl::unordered_dense::detail::wyhash::hash(data, size);
    }

    inline
    u64 Mix(u64 a, u64 b)
    {
        return ankerl::unordered_dense::detail::wyhash::mix(a, b);
    }
}

// -----------------------------------------------------------------------------
//                            CString + StringView
// -----------------------------------------------------------------------------

namespace nova
{
    template<typename CharT>
    class CString
    {
        const CharT* data = EmptyCStr;
        bool        owned = false;

    public:
        static constexpr const CharT EmptyCStr[] { 0 };

        constexpr CString() = default;

        constexpr CString(const CharT* _data, usz length, bool null_terminated)
        {
            if (null_terminated) {
                data = _data;
            } else {
                auto* cstr = new CharT[length + 1];
                std::memcpy(cstr, _data, length);
                cstr[length] = '\0';

                data = cstr;
                owned = true;
            }
        }

// -----------------------------------------------------------------------------

        CString(const CString&) = delete;
        CString& operator=(const CString&) = delete;
        CString(CString&&) = delete;
        CString& operator=(CString&&) = delete;

// -----------------------------------------------------------------------------

        constexpr operator const CharT*() const noexcept
        {
            return data;
        }

        constexpr const CharT& operator[](usz index) const noexcept
        {
            return data[index];
        }

        constexpr const CharT* const& Get() const noexcept
        {
            return data;
        }

        constexpr ~CString()
        {
            if (owned) delete[] data;
        }
    };

// -----------------------------------------------------------------------------

    // TODO: Hashing, comparison, etc..
    // TODO: One String class to rule them all with COW semantics?

    template<typename CharT>
    class BasicStringView
    {
        const CharT*   data      = CString<CharT>::EmptyCStr;
        u64          length : 63 = 0;
        u64 null_terminated :  1 = false;

        constexpr void FixIfLastIsNull() noexcept
        {
            if (length > 0 && data[length - 1] == '\0') {
                null_terminated = true;
                length--;
            }
        }

// -----------------------------------------------------------------------------

    public:
        constexpr BasicStringView() noexcept
            : data(CString<CharT>::EmptyCStr)
            , length(0)
            , null_terminated(true)
        {}

// -----------------------------------------------------------------------------

        constexpr BasicStringView(const BasicStringView& other) noexcept
            : data(other.data)
            , length(other.length)
            , null_terminated(other.null_terminated)
        {}

        constexpr BasicStringView& operator=(const BasicStringView& other) noexcept
        {
            data = other.data;
            length = other.length;
            null_terminated = other.null_terminated;
            return *this;
        }

// -----------------------------------------------------------------------------

        constexpr BasicStringView(const std::basic_string<CharT>& str) noexcept
            : data(str.data())
            , length(str.size())
            , null_terminated(true)
        {}

        static constexpr usz StrLen(const CharT* str)
        {
            auto first = str;
            for (;;) {
                if (!*str) {
                    return str - first;
                }
                str++;
            }
        }

        constexpr BasicStringView(const CharT* str) noexcept
            : data(str)
            , length(StrLen(str))
            , null_terminated(true)
        {}

// -----------------------------------------------------------------------------

        constexpr BasicStringView(std::basic_string_view<CharT> str) noexcept
            : data(str.data())
            , length(str.size())
            , null_terminated(false)
        {
            FixIfLastIsNull();
        }

        constexpr BasicStringView(const CharT* str, size_t count) noexcept
            : data(str)
            , length(count)
            , null_terminated(false)
        {
            FixIfLastIsNull();
        }

        template<usz Size>
        constexpr BasicStringView(const CharT(&str)[Size]) noexcept
            : data(str)
            , length(u64(Size))
            , null_terminated(false)
        {
            FixIfLastIsNull();
        }

// -----------------------------------------------------------------------------

        constexpr bool IsNullTerminated() const noexcept
        {
            return null_terminated;
        }

        constexpr usz Size() const noexcept
        {
            return length;
        }

        constexpr const CharT* begin() const noexcept
        {
            return data;
        }

        constexpr const CharT* end() const noexcept
        {
            return data + length;
        }

        constexpr const CharT* Data() const noexcept
        {
            return data;
        }

        constexpr CString<CharT> CStr() const
        {
            return CString<CharT>(data, length, null_terminated);
        }

// -----------------------------------------------------------------------------

        constexpr explicit operator std::basic_string<CharT>() const
        {
            return std::basic_string<CharT>(data, length);
        }

        constexpr operator std::basic_string_view<CharT>() const noexcept
        {
            return std::basic_string_view<CharT>(data, length);
        }

        constexpr operator fs::path() const noexcept
        {
            return fs::path(std::basic_string_view<CharT>(*this));
        }

// -----------------------------------------------------------------------------

        constexpr bool operator==(const BasicStringView& other) const noexcept
        {
            if (length != other.length) return false;
            return std::memcmp(data, other.data, length) == 0;
        }
    };

    template<typename CharT>
    std::basic_ostream<CharT, std::char_traits<CharT>>& operator<<(std::basic_ostream<CharT, std::char_traits<CharT>>& os, const BasicStringView<CharT>& str)
    {
        return os << std::basic_string_view<CharT>(str);
    }

    template<typename CharT>
    struct fmt::formatter<BasicStringView<CharT>> : fmt::ostream_formatter {};

    using StringView = BasicStringView<char>;
}

// -----------------------------------------------------------------------------
//                               Formatting
// -----------------------------------------------------------------------------

namespace nova
{
    inline
    std::string Fmt(StringView str)
    {
        return std::string(str);
    }

    template<typename ...Args>
    std::string Fmt(const fmt::format_string<Args...> fmt, Args&&... args)
    {
        return fmt::vformat(fmt.get(), fmt::make_format_args(args...));
    }
}

// -----------------------------------------------------------------------------
//                                 Logging
// -----------------------------------------------------------------------------

namespace nova
{
    inline
    void Log(StringView str)
    {
        fmt::println("{}", str);
    }

    template<typename ...Args>
    void Log(const fmt::format_string<Args...> fmt, Args&&... args)
    {
        fmt::println(fmt, std::forward<Args>(args)...);
    }
}

// -----------------------------------------------------------------------------
//                                Exceptions
// -----------------------------------------------------------------------------

#define NOVA_ENABLE_STACK_TRACE

namespace nova
{

    struct Exception : std::exception
    {
        struct NoStack {};

    private:
        void LogContents()
        {
            if (HasStack()) {
                Log("────────────────────────────────────────────────────────────────────────────────\n{}", Stack());
            }
            Log("────────────────────────────────────────────────────────────────────────────────\n"
                "Error: {}\n"
                "────────────────────────────────────────────────────────────────────────────────",
                What());
        }
    public:

        Exception(NoStack,
                  std::string msg,
                  std::source_location loc = std::source_location::current())
            : message(std::move(msg))
            , source_location(std::move(loc))
        {
            LogContents();
        }

        Exception(std::string msg,
                  std::source_location loc = std::source_location::current(),
                  std::stacktrace    trace = std::stacktrace::current())
            : message(std::move(msg))
            , source_location(std::move(loc))
            , back_trace(std::move(trace))
        {
            LogContents();
        }

        const char*                     What() const noexcept { return message.c_str(); }
        const std::source_location& Location() const noexcept { return source_location; }
        bool                        HasStack() const noexcept { return back_trace.has_value(); }
        const std::stacktrace&         Stack() const { return back_trace.value(); }

        const char*                     what() const noexcept { return What(); }

    private:
        std::string                         message;
        const std::source_location& source_location;
        std::optional<std::stacktrace>   back_trace;
    };
}

#define NOVA_THROW(fmt_str, ...) \
    throw ::nova::Exception(::nova::Fmt(fmt_str __VA_OPT__(,) __VA_ARGS__))

#define NOVA_THROW_STACKLESS(fmt_str, ...) \
    throw ::nova::Exception(::nova::Exception::NoStack{}, ::nova::Fmt(fmt_str __VA_OPT__(,) __VA_ARGS__));

// -----------------------------------------------------------------------------
//                              Debug Printing
// -----------------------------------------------------------------------------

#define NOVA_DEBUG() \
    ::nova::Log("    Debug :: {} - {}", __LINE__, __FILE__)

#define NOVA_FMTEXPR(expr) \
    ::nova::Fmt(#expr " = {}", (expr))

#define NOVA_DEBUGEXPR(expr) \
    ::nova::Log(NOVA_FMTEXPR(expr))

// -----------------------------------------------------------------------------
//                           Timing Debug Helper
// -----------------------------------------------------------------------------

namespace nova::detail
{
    thread_local inline std::chrono::steady_clock::time_point NovaTimeitLast;
}

#define NOVA_TIMEIT_RESET() ::nova::detail::NovaTimeitLast = std::chrono::steady_clock::now()

#define NOVA_TIMEIT(...) do {                                                  \
    using namespace std::chrono;                                               \
    ::nova::Log("- Timeit ({}) :: " __VA_OPT__("[{}] ") "{} - {}",                \
        duration_cast<milliseconds>(steady_clock::now()                        \
            - ::nova::detail::NovaTimeitLast), __VA_OPT__(__VA_ARGS__,) __LINE__, __FILE__); \
    ::nova::detail::NovaTimeitLast = steady_clock::now();                      \
} while (0)

// -----------------------------------------------------------------------------
//                                 Asserts
// -----------------------------------------------------------------------------

#define NOVA_ASSERT(condition, fmt_str, ...) do { \
    if (!(condition)) [[unlikely]]                \
        NOVA_THROW(fmt_str, __VA_ARGS__);         \
} while (0)

#define NOVA_UNREACHABLE() \
    NOVA_ASSERT(false, "Unreachable")

#define NOVA_ASSERT_NONULL(condition) \
    NOVA_ASSERT(condition, "Expected non-null: " #condition)

// -----------------------------------------------------------------------------
//                        UTF-16 String Conversions
// -----------------------------------------------------------------------------

namespace nova
{
    inline
    std::wstring ToUtf16(StringView source)
    {
        std::wstring out(source.Size(), '\0');
        auto len = simdutf::convert_utf8_to_utf16(source.Data(), source.Size(), (char16_t*)out.data());
        out.resize(len);
        return out;
    }

    inline
    std::string FromUtf16(BasicStringView<wchar_t> source)
    {
        std::string out(source.Size() * 3, '\0');
        auto len = simdutf::convert_utf16_to_utf8((const char16_t*)source.Data(), source.Size(), out.data());
        out.resize(len);
        return out;
    }
}

// -----------------------------------------------------------------------------
//                    Duration / Size String Conversions
// -----------------------------------------------------------------------------

namespace nova
{
    inline
    std::string DurationToString(std::chrono::duration<double, std::nano> dur)
    {
        f64 nanos = dur.count();

        if (nanos > 1e9) {
            f64 seconds = nanos / 1e9;
            u32 decimals = 2 - u32(std::log10(seconds));
            return Fmt("{:.{}f}s",seconds, decimals);
        }

        if (nanos > 1e6) {
            f64 millis = nanos / 1e6;
            u32 decimals = 2 - u32(std::log10(millis));
            return Fmt("{:.{}f}ms", millis, decimals);
        }

        if (nanos > 1e3) {
            f64 micros = nanos / 1e3;
            u32 decimals = 2 - u32(std::log10(micros));
            return Fmt("{:.{}f}us", micros, decimals);
        }

        if (nanos > 0) {
            u32 decimals = 2 - u32(std::log10(nanos));
            return Fmt("{:.{}f}ns", nanos, decimals);
        }

        return "0";
    }

    inline
    std::string ByteSizeToString(u64 bytes)
    {
        constexpr auto Exabyte   = 1ull << 60;
        if (bytes > Exabyte) {
            f64 exabytes = bytes / f64(Exabyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(exabytes)));
            return Fmt("{:.{}f}EiB", exabytes, decimals);
        }

        constexpr auto Petabyte  = 1ull << 50;
        if (bytes > Petabyte) {
            f64 petabytes = bytes / f64(Petabyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(petabytes)));
            return Fmt("{:.{}f}PiB", petabytes, decimals);
        }

        constexpr auto Terabyte  = 1ull << 40;
        if (bytes > Terabyte) {
            f64 terabytes = bytes / f64(Terabyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(terabytes)));
            return Fmt("{:.{}f}TiB", terabytes, decimals);
        }

        constexpr auto Gigabyte = 1ull << 30;
        if (bytes > Gigabyte) {
            f64 gigabytes = bytes / f64(Gigabyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(gigabytes)));
            return Fmt("{:.{}f}GiB", gigabytes, decimals);
        }

        constexpr auto Megabyte = 1ull << 20;
        if (bytes > Megabyte) {
            f64 megabytes = bytes / f64(Megabyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(megabytes)));
            return Fmt("{:.{}f}MiB", megabytes, decimals);
        }

        constexpr auto Kilobyte = 1ull << 10;
        if (bytes > Kilobyte) {
            f64 kilobytes = bytes / f64(Kilobyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(kilobytes)));
            return Fmt("{:.{}f}KiB", kilobytes, decimals);
        }

        if (bytes > 0) {
            u32 decimals = 2 - std::min(2u, u32(std::log10(bytes)));
            return Fmt("{:.{}f}", f64(bytes), decimals);
        }

        return "0";
    }
}

// -----------------------------------------------------------------------------
//                             Safe numeric cast
// -----------------------------------------------------------------------------

namespace nova::math
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

// -----------------------------------------------------------------------------
//                           Environment Variables
// -----------------------------------------------------------------------------

namespace nova::env
{
    std::string GetValue(StringView name);
}

// -----------------------------------------------------------------------------
//                          Nova Supplementary Stack
// -----------------------------------------------------------------------------

#ifndef NOVA_STACK_SIZE
#  define NOVA_STACK_SIZE 128ull * 1024 * 1024
#endif

namespace nova::detail
{
    struct ThreadStack
    {
        std::byte* ptr;

        std::byte* beg;
        std::byte* end;

        ThreadStack()
            : ptr(static_cast<std::byte *>(AllocVirtual(AllocationType::Commit | AllocationType::Reserve, NOVA_STACK_SIZE)))
            , beg(ptr)
            , end(ptr + NOVA_STACK_SIZE)
        {}

        ~ThreadStack()
        {
            FreeVirtual(FreeType::Decommit, ptr);
        }

        size_t RemainingBytes()
        {
            return size_t(end - ptr);
        }
    };

    NOVA_FORCE_INLINE
    ThreadStack& GetThreadStack()
    {
        thread_local ThreadStack NovaStack;
        return NovaStack;
    }

    class ThreadStackPoint
    {
        std::byte* ptr;

    public:
        ThreadStackPoint() noexcept
            : ptr(GetThreadStack().ptr)
        {}

        ~ThreadStackPoint() noexcept
        {
            GetThreadStack().ptr = ptr;
        }

        ThreadStackPoint(const ThreadStackPoint&) = delete;
        auto   operator=(const ThreadStackPoint&) = delete;
        ThreadStackPoint(ThreadStackPoint&&) = delete;
        auto   operator=(ThreadStackPoint&&) = delete;
    };

    template<typename T>
    T* StackAlloc(usz count)
    {
        auto& stack = GetThreadStack();
        T* ptr = reinterpret_cast<T*>(stack.ptr);
        stack.ptr = AlignUpPower2(stack.ptr + sizeof(T) * count, 16);
        return ptr;
    }
}

#define NOVA_STACK_POINT()            ::nova::detail::ThreadStackPoint NOVA_UNIQUE_VAR()
#define NOVA_STACK_ALLOC(type, count) ::nova::detail::StackAlloc<type>(count)

// -----------------------------------------------------------------------------
//                         Reference Counting Pointer
// -----------------------------------------------------------------------------

#define NOVA_SAFE_REFERENCES 1

int32_t& RefTotal();

namespace nova::types
{
    class RefCounted
    {
        static size_t& RefTotal()
        {
            static size_t ref_total = 0;
            return ref_total;
        }
    private:
        u32 reference_count = 0;
        static constexpr u32 InvalidRefCount = ~0u;

    protected:
        ~RefCounted() = default;

    public:
        RefCounted() = default;

        void RefCounted_Acquire()
        {
#ifdef NOVA_SAFE_REFERENCES
            if (std::atomic_ref<u32>(reference_count) == InvalidRefCount) [[unlikely]] {
                NOVA_THROW("Attempted to Acquire on RefCounted object that is being destroyed!");
            }
#endif
            ++std::atomic_ref<u32>(reference_count);
        }

        bool RefCounted_Release()
        {
            bool to_delete = !--std::atomic_ref<u32>(reference_count);
#ifdef NOVA_SAFE_REFERENCES
            if (to_delete) {
                std::atomic_ref<u32>(reference_count).store(InvalidRefCount);
            }
#endif
            return to_delete;
        }

        // RefCounted objects are STRICTLY unique and pointer stable
        RefCounted(RefCounted&&) = delete;
        RefCounted(const RefCounted&) = delete;
        RefCounted& operator=(RefCounted&&) = delete;
        RefCounted& operator=(const RefCounted&) = delete;
    };

// -----------------------------------------------------------------------------
//                          Reference counted pointer
// -----------------------------------------------------------------------------

    template<typename T>
    class Ref
    {
        T* value{};

    public:

// -----------------------------------------------------------------------------

        Ref()
            : value(nullptr)
        {

        }

        ~Ref()
        {
            if (value && value->RefCounted_Release()) {
                delete value;
            }
        }

// -----------------------------------------------------------------------------

        template<typename... Args>
        static Ref<T> Create(Args&&... args)
        {
            return Ref(new T(std::forward<Args>(args)...));
        }

// -----------------------------------------------------------------------------

        Ref(T* value)
            : value(value)
        {
            if (value) {
                value->RefCounted_Acquire();
            }
        }

// -----------------------------------------------------------------------------

        Ref(Ref<T>&& moved)
            : value(moved.value)
        {
#ifdef NOVA_SAFE_REFERENCES
            NOVA_ASSERT(this != &moved, "Ref::Ref(Ref<T>&&) called on self");
#endif
            moved.value = nullptr;
        }

        Ref<T>& operator=(Ref<T>&& moved) noexcept
        {
            if (value != moved.value) {
                this->~Ref();
                new (this) Ref(moved);
            }
            return *this;
        }

// -----------------------------------------------------------------------------

        Ref(const Ref<T>& copied)
            : value(copied.value)
        {
#ifdef NOVA_SAFE_REFERENCES
            NOVA_ASSERT(this != &copied, "Ref::Ref(const Ref<T>&) called on self");
#endif
            if (value) {
                value->RefCounted_Acquire();
            }
        }

        Ref<T>& operator=(const Ref<T>& copied)
        {
            if (value != copied.value) {
                this->~Ref();
                new (this) Ref(copied);
            }
            return *this;
        }

// -----------------------------------------------------------------------------

        bool HasValue() const noexcept
        {
            return value;
        }

        auto operator==(const Ref<T>& other) const
        {
            return value == other.value;
        }

        auto operator!=(const Ref<T>& other) const
        {
            return value != other.value;
        }

        auto operator==(T* other) const
        {
            return value == other;
        }

        auto operator!=(T* other) const
        {
            return value != other;
        }

// -----------------------------------------------------------------------------

        operator T&() &
        {
            return *value;
        }

        operator T*() &
        {
            return value;
        }

// -----------------------------------------------------------------------------

        template<typename T2>
        requires std::derived_from<T, T2>
        operator Ref<T2>() const
        {
            return Ref<T2>(value);
        }

        template<typename T2>
        requires std::derived_from<T, T2>
        Ref<T2> ToBase()
        {
            return Ref<T2>(value);
        }

// -----------------------------------------------------------------------------

        template<typename T2>
        Ref<T2> As() const
        {
#ifdef NOVA_SAFE_REFERENCES
            auto cast = dynamic_cast<T2*>(value);
            if (value && !cast) {
                NOVA_THROW("Invalid cast!");
            }
            return Ref<T2>(cast);
#else
            return Ref<T2>(static_cast<T2*>(value));
#endif
        }

        template<typename T2>
        Ref<T2> TryAs() const
        {
            return Ref<T2>(dynamic_cast<T2*>(value));
        }

        template<typename T2>
        bool IsA() const
        {
            return dynamic_cast<T2*>(value);
        }

// -----------------------------------------------------------------------------

        T* operator->() const
        {
#ifdef NOVA_SAFE_REFERENCES
            if (!value) {
                NOVA_THROW("Ref<{}>::operator-> called on null reference", typeid(T).name());
            }
#endif
            return value;
        }

        T& operator*() const
        {
#ifdef NOVA_SAFE_REFERENCES
            if (!value) {
                NOVA_THROW("Ref<{}>::operator* called on null reference", typeid(T).name());
            }
#endif
            return *value;
        }

        T* Raw() const
        {
            return value;
        }
    };
}

template<typename T>
struct ankerl::unordered_dense::hash<nova::Ref<T>> {
    using is_avalanching = void;
    nova::types::u64 operator()(const nova::Ref<T>& key) const noexcept {
        return detail::wyhash::hash(nova::types::u64(key.Raw()));
    }
};

// -----------------------------------------------------------------------------