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
    }

    using namespace types;

// -----------------------------------------------------------------------------

    template<typename... Ts>
    struct Overloads : Ts... {
        using Ts::operator()...;
    };

    template<typename... Ts> Overloads(Ts...) -> Overloads<Ts...>;

    // -----------------------------------------------------------------------------

    template<typename K, typename V>
    using HashMap = ankerl::unordered_dense::map<K, V>;

    // -----------------------------------------------------------------------------

    template<typename T>
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

namespace nova
{

// -----------------------------------------------------------------------------

#define NOVA_CONCAT_INTERNAL(a, b) a##b
#define NOVA_CONCAT(a, b) NOVA_CONCAT_INTERNAL(a, b)

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

    namespace detail
    {
        thread_local inline std::string NovaFormatOutput;
    }

#define NOVA_FORMAT_TEMP(fmt, ...) ([&]() -> const std::string& {                                \
    ::nova::detail::NovaFormatOutput.clear();                                                            \
    std::format_to(std::back_inserter(::nova::detail::NovaFormatOutput), fmt __VA_OPT__(,) __VA_ARGS__); \
    return ::nova::detail::NovaFormatOutput;                                                             \
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

    template<class Ret, class... Types>
    struct FuncBase {
        void* body;
        Ret(*fptr)(void*, Types...);

        Ret operator()(Types... args) {
            return fptr(body, std::forward<Types>(args)...);
        }
    };

    template<class Tx>
    struct GetFunctionImpl {};

    template<class Ret, class... Types>
    struct GetFunctionImpl<Ret(Types...)> { using type = FuncBase<Ret, Types...>; };

    template<class Sig>
    struct LambdaRef : GetFunctionImpl<Sig>::type {
        template<class Fn>
        LambdaRef(Fn&& fn)
            : GetFunctionImpl<Sig>::type(&fn,
                [](void*b, auto... args) -> auto {
                    return (*(Fn*)b)(args...);
                })
        {};
    };

// -----------------------------------------------------------------------------

    struct RawByteView
    {
        const void* data;
        usz   size;

        template<class T>
        RawByteView(const T& t)
            : data((const void*)&t), size(sizeof(T))
        {}

        RawByteView(const void* data, usz size)
            : data(data), size(size)
        {}
    };

// -----------------------------------------------------------------------------

    template<typename T>
    T* Temp(T&& v)
    {
        return &v;
    }

// -----------------------------------------------------------------------------

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
    };

// -----------------------------------------------------------------------------

#define NOVA_NO_INLINE __declspec(noinline)
#define NOVA_FORCE_INLINE __forceinline
}