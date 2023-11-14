#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Math.hpp>

#include <simdutf.h>

#ifndef NOVA_STACK_SIZE
#  define NOVA_STACK_SIZE 4 * 1024 * 1024
#endif

namespace nova
{
    namespace detail
    {
        struct ThreadStack
        {
            std::unique_ptr<std::byte[]> stack = std::make_unique<std::byte[]>(NOVA_STACK_SIZE);
            std::byte*                     ptr = stack.get();
        };

        inline thread_local ThreadStack NovaStack;

        class ThreadStackPoint
        {
            std::byte* ptr;

        public:
            ThreadStackPoint() noexcept
                : ptr(detail::NovaStack.ptr)
            {}

            ~ThreadStackPoint() noexcept
            {
                detail::NovaStack.ptr = ptr;
            }

            ThreadStackPoint(const ThreadStackPoint&) = delete;
            auto   operator=(const ThreadStackPoint&) = delete;
            ThreadStackPoint(ThreadStackPoint&&) = delete;
            auto   operator=(ThreadStackPoint&&) = delete;
        };
    }

    template<class T>
    NOVA_FORCE_INLINE
    T* StackAlloc(usz count)
    {
        T* ptr = reinterpret_cast<T*>(detail::NovaStack.ptr);
        detail::NovaStack.ptr = nova::AlignUpPower2(detail::NovaStack.ptr + sizeof(T) * count, 16);
        return ptr;
    }

    template<class ...Args>
    std::string_view StackFormat(const std::format_string<Args...> fmt, Args&&... args)
    {
        char* begin = reinterpret_cast<char*>(detail::NovaStack.ptr);
        char* end = std::vformat_to(begin, fmt.get(), std::make_format_args(std::forward<Args>(args)...));
        detail::NovaStack.ptr = nova::AlignUpPower2(reinterpret_cast<std::byte*>(end), 16);
        return std::string_view { begin, end };
    }

    inline
    std::wstring_view StackToUtf16(std::string_view source)
    {
        char16_t* begin = reinterpret_cast<char16_t*>(detail::NovaStack.ptr);
        auto size = simdutf::convert_utf8_to_utf16(source.data(), source.size(), begin);
        detail::NovaStack.ptr = nova::AlignUpPower2(detail::NovaStack.ptr + size * sizeof(char16_t), 16);
        return { reinterpret_cast<wchar_t*>(begin), size };
    }

    inline
    std::string_view StackFromUtf16(std::wstring_view source)
    {
        char* begin = reinterpret_cast<char*>(detail::NovaStack.ptr);
        auto size = simdutf::convert_utf16_to_utf8(reinterpret_cast<const char16_t*>(source.data()), source.size(), begin);
        detail::NovaStack.ptr = nova::AlignUpPower2(detail::NovaStack.ptr + size, 16);
        return { begin, size };
    }

    inline
    const char* StackToCStr(std::string_view source)
    {
        auto cstr = StackAlloc<char>(source.size() + 1);
        std::memcpy(cstr, source.data(), source.size());
        cstr[source.size()] = '\0';
        return cstr;
    }

    inline
    const wchar_t* StackToCStr(std::wstring_view source)
    {
        auto cstr = StackAlloc<wchar_t>(source.size() + 1);
        std::wmemcpy(cstr, source.data(), source.size());
        cstr[source.size()] = L'\0';
        return cstr;
    }

#define NOVA_STACK_POINT() ::nova::detail::ThreadStackPoint NOVA_CONCAT(nova_stack_point_, __LINE__)
#define NOVA_STACK_ALLOC(type, count) ::nova::StackAlloc<type>(count)
#define NOVA_STACK_FORMAT(...) ::nova::StackFormat(__VA_ARGS__)
#define NOVA_STACK_TO_UTF16(source) ::nova::StackToUtf16(source)
#define NOVA_STACK_FROM_UTF16(source) ::nova::StackFromUtf16(source)
#define NOVA_STACK_TO_CSTR(str) ::nova::StackToCStr(str)
}