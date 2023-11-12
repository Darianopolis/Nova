#pragma once

namespace nova
{
#ifdef NOVA_USE_EXPLICIT_STACK

    struct ThreadSupplementaryStack
    {
        std::array<byte, 1024 * 1024> stack;
        byte* ptr = &stack[0];
    };

    namespace detail
    {
        inline thread_local ThreadSupplementaryStack NovaStack;
    }

    template<typename T>
    class StackPtr
    {
        T* ptr;
    public:
        StackPtr(usz count)
        {
            ptr = (T*)detail::NovaStack.ptr;
            detail::NovaStack.ptr += sizeof(T) * count;
        }

        ~StackPtr()
        {
            // NOVA_ASSERT(NovaStack.ptr >= (byte*)ptr, "Out of order stack free");
            detail::NovaStack.ptr = (byte*)ptr;
        }

        StackPtr(const StackPtr<T>&) = delete;
        auto operator=(const StackPtr<T>&) = delete;
        StackPtr(StackPtr<T>&&) = delete;
        auto operator=(StackPtr<T>&&) = delete;

        T& operator*() { return *ptr; }
        T* operator->() { return ptr; }
        T& operator[](usz i) { return ptr[i]; }

        operator T*() { return ptr; }
    };

#define NOVA_ALLOC_STACK(type, count) StackPtr<type>(count)

#else

#define NOVA_ALLOC_STACK(type, count)                                          \
    (type*)alloca(sizeof(type) * count)

#endif
}