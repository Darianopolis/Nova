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
#include <bitset>
#include <stacktrace>

#include "Vendor/Nova_Vendor.hpp"

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
    }

    using namespace types;

// -----------------------------------------------------------------------------

#define NOVA_DECORATE_FLAG_ENUM(enumType)                                \
    inline enumType operator|(enumType l, enumType r) {                  \
        return enumType(static_cast<std::underlying_type_t<enumType>>(l) \
            | static_cast<std::underlying_type_t<enumType>>(r));         \
    }                                                                    \
    inline bool operator>=(enumType l, enumType r) {               \
        return static_cast<std::underlying_type_t<enumType>>(r)          \
            == (static_cast<std::underlying_type_t<enumType>>(l)         \
                & static_cast<std::underlying_type_t<enumType>>(r));     \
    }                                                                    \
    inline bool operator&(enumType l, enumType r) {                \
        return static_cast<std::underlying_type_t<enumType>>(0)          \
            != (static_cast<std::underlying_type_t<enumType>>(l)         \
                & static_cast<std::underlying_type_t<enumType>>(r));     \
    }

// -----------------------------------------------------------------------------

#define NOVA_DEBUG() std::cout << std::format("    Debug :: {} - {}\n", __LINE__, __FILE__)

#define NOVA_LOG(fmt, ...) \
    std::cout << std::format(fmt"\n" __VA_OPT__(,) __VA_ARGS__)

#define NOVA_LOGEXPR(expr) do {           \
    std::osyncstream sso(std::cout);      \
    sso << #expr " = " << (expr) << '\n'; \
} while (0)

#define NOVA_THROW(fmt, ...) do {                          \
    auto msg = std::format(fmt __VA_OPT__(,) __VA_ARGS__); \
    std::cout << std::stacktrace::current();               \
    std::cout << std::format("\nERROR: {}\n", msg);        \
    throw std::runtime_error(std::move(msg));              \
} while (0)

// -----------------------------------------------------------------------------

    thread_local inline std::chrono::steady_clock::time_point PyrTimeitLast;

#define NOVA_TIMEIT_RESET() ::nova::PyrTimeitLast = std::chrono::steady_clock::now()

#define NOVA_TIMEIT(...) do {                                                       \
    using namespace std::chrono;                                                    \
    std::cout << std::format("- Timeit ({}) :: " __VA_OPT__("[{}] ") "{} - {}\n",   \
        duration_cast<milliseconds>(steady_clock::now()                             \
            - ::nova::PyrTimeitLast), __VA_OPT__(__VA_ARGS__,) __LINE__, __FILE__); \
    ::nova::PyrTimeitLast = steady_clock::now();                                    \
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
#define NOVA_ON_SCOPE_EXIT(...)    ::nova::OnScopeExit    NOVA_CONCAT(_scope_guard_$_, __COUNTER__) = [__VA_ARGS__]
#define NOVA_ON_SCOPE_SUCCESS(...) ::nova::OnScopeSuccess NOVA_CONCAT(_scope_guard_$_, __COUNTER__) = [__VA_ARGS__]
#define NOVA_ON_SCOPE_FAILURE(...) ::nova::OnScopeFailure NOVA_CONCAT(_scope_guard_$_, __COUNTER__) = [__VA_ARGS__]

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
}