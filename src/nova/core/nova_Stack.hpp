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

        std::byte* beg;
        std::byte* end;

        ThreadStack();
        ~ThreadStack();

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
        ThreadStackPoint() noexcept;
        ~ThreadStackPoint() noexcept;

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