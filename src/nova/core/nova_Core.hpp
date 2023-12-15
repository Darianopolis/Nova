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

using namespace std::literals;

namespace nova
{
    namespace types
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

    using namespace types;

// -----------------------------------------------------------------------------

    template<typename... Ts>
    struct Overloads : Ts... {
        using Ts::operator()...;
    };

    template<typename... Ts> Overloads(Ts...) -> Overloads<Ts...>;

// -----------------------------------------------------------------------------

    template<typename T>
    T* Temp(T&& v)
    {
        return &v;
    }
}

// -----------------------------------------------------------------------------

#define NOVA_CONCAT_INTERNAL(a, b) a##b
#define NOVA_CONCAT(a, b) NOVA_CONCAT_INTERNAL(a, b)
#define NOVA_UNIQUE_VAR() NOVA_CONCAT(nova_var, __COUNTER__)

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

#ifdef NOVA_PLATFORM_WINDOWS
#include "win32/nova_Win32Core.hpp"
#endif