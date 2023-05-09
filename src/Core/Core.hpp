#pragma once

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

#include "Vendor.hpp"

using namespace std::literals;

namespace pyr
{
    using u8  = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;

    using i8  = int8_t;
    using i16 = int16_t;
    using i32 = int32_t;
    using i64 = int64_t;

    using c8  = char;
    using c16 = wchar_t;
    using c32 = char32_t;

    using f32 = float;
    using f64 = double;

    using b8 = bool;
    using b32 = uint32_t;

    using usz = size_t;

    using byte = std::byte;

// -----------------------------------------------------------------------------

    using glm::vec2;
    using glm::ivec2;
    using glm::uvec2;

    using glm::vec3;
    using glm::ivec3;
    using glm::uvec3;

    using glm::vec4;
    using glm::uvec4;
    using glm::ivec4;

    using glm::quat;

    using glm::mat3;
    using glm::mat4;

// -----------------------------------------------------------------------------

#define PYR_DECORATE_FLAG_ENUM(enumType)                                 \
    inline enumType operator|(enumType l, enumType r) {                  \
        return enumType(static_cast<std::underlying_type_t<enumType>>(l) \
            | static_cast<std::underlying_type_t<enumType>>(r));         \
    }                                                                    \
    inline pyr::b8 operator>=(enumType l, enumType r) {                  \
        return static_cast<std::underlying_type_t<enumType>>(r)          \
            == (static_cast<std::underlying_type_t<enumType>>(l)         \
                & static_cast<std::underlying_type_t<enumType>>(r));     \
    }                                                                    \
    inline pyr::b8 operator&(enumType l, enumType r) {                   \
        return static_cast<std::underlying_type_t<enumType>>(0)          \
            != (static_cast<std::underlying_type_t<enumType>>(l)         \
                & static_cast<std::underlying_type_t<enumType>>(r));     \
    }

// -----------------------------------------------------------------------------

#define PYR_DEBUG() std::cout << std::format("    Debug :: {} - {}\n", __LINE__, __FILE__)

#define PYR_LOG(fmt, ...) \
    std::cout << std::format(fmt"\n" __VA_OPT__(,) __VA_ARGS__)

#define PYR_LOGEXPR(expr) do {            \
    std::osyncstream sso(std::cout);      \
    sso << #expr " = " << (expr) << '\n'; \
} while (0)

#define PYR_THROW(fmt, ...) do {                                  \
    auto msg = std::format(fmt __VA_OPT__(,) __VA_ARGS__);        \
    std::cout << std::format("ERROR: {}\n  Location = {} @ {}\n", \
        msg, __LINE__, __FILE__);                                 \
    throw std::runtime_error(std::move(msg));                     \
} while (0)

// -----------------------------------------------------------------------------

    thread_local inline std::chrono::steady_clock::time_point PyrTimeitLast;

#define PYR_TIMEIT_RESET() ::pyr::PyrTimeitLast = std::chrono::steady_clock::now()

#define PYR_TIMEIT(...) do {                                                       \
    using namespace std::chrono;                                                   \
    std::cout << std::format("- Timeit ({}) :: " __VA_OPT__("[{}] ") "{} - {}\n",  \
        duration_cast<milliseconds>(steady_clock::now()                            \
            - ::pyr::PyrTimeitLast), __VA_OPT__(__VA_ARGS__,) __LINE__, __FILE__); \
    ::pyr::PyrTimeitLast = steady_clock::now();                                    \
} while (0)

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

#define PYR_DO_ONCE(...) static ::pyr::DoOnceState PYR_CONCAT(_do_once_$_, __COUNTER__) = [__VA_ARGS__]
#define PYR_ON_EXIT(...) static ::pyr::OnShutdown  PYR_CONCAT(_on_exit_$_, __COUNTER__) = [__VA_ARGS__]

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

#define PYR_CONCAT_INTERNAL(a, b) a##b
#define PYR_CONCAT(a, b) PYR_CONCAT_INTERNAL(a, b)
#define PYR_ON_SCOPE_EXIT(...)    OnScopeExit    PYR_CONCAT(_scope_guard_$_, __COUNTER__) = [__VA_ARGS__]
#define PYR_ON_SCOPE_SUCCESS(...) OnScopeSuccess PYR_CONCAT(_scope_guard_$_, __COUNTER__) = [__VA_ARGS__]
#define PYR_ON_SCOPE_FAILURE(...) OnScopeFailure PYR_CONCAT(_scope_guard_$_, __COUNTER__) = [__VA_ARGS__]

// -----------------------------------------------------------------------------

    template<class T>
    T* Temp(T&& v)
    {
        return &v;
    }

    inline
    void Error(std::string msg)
    {
        std::cout << msg +"\n";
        throw std::runtime_error(msg);
    }

// -----------------------------------------------------------------------------

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