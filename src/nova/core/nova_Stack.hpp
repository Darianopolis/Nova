#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Math.hpp>

#include <fmt/format.h>

#include <simdutf.h>

#ifndef NOVA_STACK_SIZE
#  define NOVA_STACK_SIZE 128ull * 1024 * 1024
#endif

namespace nova::detail
{
    struct ThreadStack
    {
        std::byte* ptr;

        ThreadStack();
        ~ThreadStack();
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
        ThreadStackPoint() noexcept;
        ~ThreadStackPoint() noexcept;

        ThreadStackPoint(const ThreadStackPoint&) = delete;
        auto   operator=(const ThreadStackPoint&) = delete;
        ThreadStackPoint(ThreadStackPoint&&) = delete;
        auto   operator=(ThreadStackPoint&&) = delete;
    };

    template<class T>
    T* StackAlloc(usz count)
    {
        auto& stack = GetThreadStack();
        T* ptr = reinterpret_cast<T*>(stack.ptr);
        stack.ptr = AlignUpPower2(stack.ptr + sizeof(T) * count, 16);
        return ptr;
    }

    template<class ...Args>
    std::string_view StackFormat(const fmt::format_string<Args...> fmt, Args&&... args)
    {
        auto& stack = GetThreadStack();
        char* begin = reinterpret_cast<char*>(stack.ptr);
        char* end = fmt::vformat_to(begin, fmt.get(), fmt::make_format_args(args...));
        *end = '\0';
        stack.ptr = AlignUpPower2(reinterpret_cast<std::byte*>(end + 1), 16);
        return std::string_view { begin, end };
    }

    inline
    std::wstring_view StackToUtf16(std::string_view source)
    {
        auto& stack = GetThreadStack();
        char16_t* begin = reinterpret_cast<char16_t*>(stack.ptr);
        auto size = simdutf::convert_utf8_to_utf16(source.data(), source.size(), begin);
        begin[size] = L'\0';
        stack.ptr = AlignUpPower2(stack.ptr + (size + 1) * sizeof(char16_t), 16);
        return { reinterpret_cast<wchar_t*>(begin), size };
    }

    inline
    std::string_view StackFromUtf16(std::wstring_view source)
    {
        auto& stack = GetThreadStack();
        auto* begin = reinterpret_cast<char*>(stack.ptr);
        auto size = simdutf::convert_utf16_to_utf8(reinterpret_cast<const char16_t*>(source.data()), source.size(), begin);
        begin[size] = '\0';
        stack.ptr = AlignUpPower2(stack.ptr + size + 1, 16);
        return { begin, size };
    }

    inline
    char* StackToCStr(std::string_view source)
    {
        auto* cstr = StackAlloc<char>(source.size() + 1);
        std::memcpy(cstr, source.data(), source.size());
        cstr[source.size()] = '\0';
        return cstr;
    }

    inline
    wchar_t* StackToCStr(std::wstring_view source)
    {
        auto cstr = StackAlloc<wchar_t>(source.size() + 1);
        std::wmemcpy(cstr, source.data(), source.size());
        cstr[source.size()] = L'\0';
        return cstr;
    }
}

#define NOVA_STACK_POINT()            ::nova::detail::ThreadStackPoint NOVA_UNIQUE_VAR()
#define NOVA_STACK_ALLOC(type, count) ::nova::detail::StackAlloc<type>(count)
#define NOVA_STACK_FORMAT(...)        ::nova::detail::StackFormat(__VA_ARGS__)
#define NOVA_STACK_TO_UTF16(source)   ::nova::detail::StackToUtf16(source)
#define NOVA_STACK_FROM_UTF16(source) ::nova::detail::StackFromUtf16(source)
#define NOVA_STACK_TO_CSTR(str)       ::nova::detail::StackToCStr(str)