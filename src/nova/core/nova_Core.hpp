#pragma once

// #define WINVER 0x0A00
// #define _WIN32_WINNT 0x0A00

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
#include <format>
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

#include "nova_Vendor.hpp"

using namespace std::literals;

namespace nova
{
    namespace types
    {
        using u8  = uint8_t;
        using u16 = uint16_t;
        using u32 = uint32_t;
        using u64 = uint64_t;

        using i8  = int8_t;
        using i16 = int16_t;
        using i32 = int32_t;
        using i64 = int64_t;

        using uc8 = unsigned char;
        using c8  = char;
        using c16 = wchar_t;
        using c32 = char32_t;

        using f32 = float;
        using f64 = double;

        using b8 = std::byte;

        using usz = size_t;

// -----------------------------------------------------------------------------

        using Vec2 = glm::vec2;
        using Vec2I = glm::ivec2;
        using Vec2U = glm::uvec2;

        using Vec3 = glm::vec3;
        using Vec3I = glm::ivec3;
        using Vec3U = glm::uvec3;

        using Vec4 = glm::vec4;
        using Vec4I = glm::ivec4;
        using Vec4U = glm::uvec4;

        using Quat = glm::quat;

        using Mat3 = glm::mat3;
        using Mat4 = glm::mat4;

        struct Trs
        {
            Vec3 translation = Vec3(0.f);
            Quat    rotation = Vec3(0.f);
            Vec3       scale = Vec3(1.f);

            Trs Copy() const noexcept
            {
                return *this;
            }

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
        };

        struct Rect2D
        {
            Vec2I offset = {};
            Vec2U extent = {};
        };

// -----------------------------------------------------------------------------

        template<class... Ts>
        struct Overloads : Ts... {
            using Ts::operator()...;
        };

        template<class... Ts> Overloads(Ts...) -> Overloads<Ts...>;

// -----------------------------------------------------------------------------

        template<class T>
        struct Span
        {
            std::span<const T> span;

        public:
            Span(std::initializer_list<T>&&      init) : span(init.begin(), init.end()) {}
            Span(const std::vector<T>&      container) : span(container.begin(), container.end()) {}
            template<usz Size>
            Span(const std::array<T, Size>& container) : span(container.begin(), container.end()) {}
            Span(const std::span<const T>&  container) : span(container.begin(), container.end()) {}

            Span(const T* first, usz count) : span(first, count) {}

        public:
            const T& operator[](usz i) const noexcept { return span.begin()[i]; }

            Span(const Span& other) noexcept : span(other.span) {}
            Span& operator=(const Span& other) noexcept { span = other.span; }

            decltype(auto) begin() const noexcept { return span.data(); }
            decltype(auto) end()   const noexcept { return span.data() + span.size(); }

            const T* data() const noexcept { return span.data(); }
            usz size() const noexcept { return span.size(); }
        };
    }

    using namespace types;

// -----------------------------------------------------------------------------

#define NOVA_DECORATE_FLAG_ENUM(enumType)                                \
    inline enumType operator|(enumType l, enumType r) {                  \
        return enumType(static_cast<std::underlying_type_t<enumType>>(l) \
            | std::to_underlying(r));                                    \
    }                                                                    \
    inline bool operator>=(enumType l, enumType r) {                     \
        return std::to_underlying(r)                                     \
            == (std::to_underlying(l) & std::to_underlying(r));          \
    }                                                                    \
    inline bool operator&(enumType l, enumType r) {                      \
        return static_cast<std::underlying_type_t<enumType>>(0)          \
            != (std::to_underlying(l) & std::to_underlying(r));          \
    }

// -----------------------------------------------------------------------------

    thread_local inline std::string NovaFormatOutput;

#define NOVA_FORMAT_TEMP(fmt, ...) ([&]() -> const std::string& {                                \
    ::nova::NovaFormatOutput.clear();                                                            \
    std::format_to(std::back_inserter(::nova::NovaFormatOutput), fmt __VA_OPT__(,) __VA_ARGS__); \
    return ::nova::NovaFormatOutput;                                                             \
}())

#define NOVA_DEBUG() \
    std::cout << std::format("    Debug :: {} - {}\n", __LINE__, __FILE__)

#define NOVA_LOG(fmt, ...) \
    std::cout << std::format(fmt"\n" __VA_OPT__(,) __VA_ARGS__)

#define NOVA_LOGEXPR(expr) do {           \
    std::osyncstream sso(std::cout);      \
    sso << #expr " = " << (expr) << '\n'; \
} while (0)

#define NOVA_THROW(fmt, ...) do {                          \
    auto msg = std::format(fmt __VA_OPT__(,) __VA_ARGS__); \
    std::cout << std::stacktrace::current();               \
    NOVA_LOG("\nERROR: {}", msg);                          \
    throw std::runtime_error(std::move(msg));              \
} while (0)

#define NOVA_ASSERT(condition, fmt, ...) do { \
    if (!(condition)) [[unlikely]]            \
        NOVA_THROW(fmt, __VA_ARGS__);         \
} while (0)

#define NOVA_ASSERT_NONULL(condition) \
    NOVA_ASSERT(condition, "Expected non-null: " #condition)

// -----------------------------------------------------------------------------

    thread_local inline std::chrono::steady_clock::time_point NovaTimeitLast;

#define NOVA_TIMEIT_RESET() ::nova::NovaTimeitLast = std::chrono::steady_clock::now()

// #define NOVA_TIMEIT(...) do {                                                  \
//     using namespace std::chrono;                                               \
//     NOVA_LOG("- Timeit ({}) :: " __VA_OPT__("[{}] ") "{} - {}",                \
//         duration_cast<milliseconds>(steady_clock::now()                        \
//             - ::nova::NovaTimeitLast), __VA_OPT__(__VA_ARGS__,) __LINE__, __FILE__); \
//     ::nova::NovaTimeitLast = steady_clock::now();                              \
// } while (0)

#define NOVA_TIMEIT(...)

// -----------------------------------------------------------------------------

    template<typename Fn>
    struct DoOnceState
    {
        DoOnceState(Fn&& fn)
        {
            fn();
        }
    };

    template<typename Fn>
    struct OnShutdown
    {
        Fn fn;

        OnShutdown(Fn&& _fn)
            : fn(std::move(_fn))
        {}

        ~OnShutdown()
        {
            fn();
        }
    };

#define NOVA_DO_ONCE(...) static ::nova::DoOnceState NOVA_CONCAT(_do_once_$_, __COUNTER__) = [__VA_ARGS__]
#define NOVA_ON_EXIT(...) static ::nova::OnShutdown  NOVA_CONCAT(_on_exit_$_, __COUNTER__) = [__VA_ARGS__]

// -----------------------------------------------------------------------------

    template<typename Fn>
    class OnScopeExit
    {
        Fn fn;

    public:
        OnScopeExit(Fn fn)
            : fn(std::move(fn))
        {}

        ~OnScopeExit()
        {
            fn();
        }
    };

    template<typename Fn>
    class OnScopeSuccess
    {
        Fn fn;
        i32 exceptions;

    public:
        OnScopeSuccess(Fn fn)
            : fn(std::move(fn))
            , exceptions(std::uncaught_exceptions())
        {}

        ~OnScopeSuccess()
        {
            if (std::uncaught_exceptions() <= exceptions)
                fn();
        }
    };

    template<typename Fn>
    class OnScopeFailure
    {
        Fn fn;
        i32 exceptions;

    public:
        OnScopeFailure(Fn fn)
            : fn(std::move(fn))
            , exceptions(std::uncaught_exceptions())
        {}

        ~OnScopeFailure()
        {
            if (std::uncaught_exceptions() > exceptions)
                fn();
        }
    };

#define NOVA_CONCAT_INTERNAL(a, b) a##b
#define NOVA_CONCAT(a, b) NOVA_CONCAT_INTERNAL(a, b)
#define NOVA_ON_SCOPE_EXIT(...)    ::nova::OnScopeExit    NOVA_CONCAT(scope_exit_callback_, __COUNTER__) = [__VA_ARGS__]
#define NOVA_ON_SCOPE_SUCCESS(...) ::nova::OnScopeSuccess NOVA_CONCAT(scope_exit_callback_, __COUNTER__) = [__VA_ARGS__]
#define NOVA_ON_SCOPE_FAILURE(...) ::nova::OnScopeFailure NOVA_CONCAT(scope_exit_callback_, __COUNTER__) = [__VA_ARGS__]

// -----------------------------------------------------------------------------

    template<class T>
    T* Temp(T&& v)
    {
        return &v;
    }

// -----------------------------------------------------------------------------

#define NOVA_DEFAULT_MOVE_DECL(type)                   \
    type(type&& other) noexcept;                       \
    type& operator=(type&& other)                      \
    {                                                  \
        if (this != &other)                            \
        {                                              \
            std::destroy_at(this);                     \
            std::construct_at(this, std::move(other)); \
        }                                              \
        return *this;                                  \
    }

// -----------------------------------------------------------------------------

#define NOVA_NO_INLINE __declspec(noinline)
#define NOVA_FORCE_INLINE __forceinline

// -----------------------------------------------------------------------------

#define NOVA_ALLOC_STACK(type, count) \
    (type*)alloca(sizeof(type) * count)

//     struct ThreadSupplementaryStack
//     {
//         std::array<byte, 1024 * 1024> stack;
//         byte* ptr = &stack[0];
//     };

//     inline thread_local ThreadSupplementaryStack NovaStack;

//     template<class T>
//     class StackPtr
//     {
//         T* ptr;
//     public:
//         StackPtr(usz count)
//         {
//             ptr = (T*)NovaStack.ptr;
//             NovaStack.ptr += sizeof(T) * count;
//         }

//         ~StackPtr()
//         {
//             // NOVA_ASSERT(NovaStack.ptr >= (byte*)ptr, "Out of order stack free");
//             NovaStack.ptr = (byte*)ptr;
//         }

//         StackPtr(const StackPtr<T>&) = delete;
//         auto operator=(const StackPtr<T>&) = delete;
//         StackPtr(StackPtr<T>&&) = delete;
//         auto operator=(StackPtr<T>&&) = delete;

//         T& operator*() { return *ptr; }
//         T* operator->() { return ptr; }
//         T& operator[](usz i) { return ptr[i]; }

//         operator T*() { return ptr; }
//     };

// #define NOVA_ALLOC_STACK(type, count) StackPtr<type>(count)
}