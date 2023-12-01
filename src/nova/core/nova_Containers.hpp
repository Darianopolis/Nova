#pragma once

#include "nova_Core.hpp"

#include <ankerl/unordered_dense.h>

namespace nova
{
// -----------------------------------------------------------------------------
//                                  Span
// -----------------------------------------------------------------------------

    template<class T>
    concept IsContiguousContainer = requires(T t)
    {
        { t.begin() } -> std::random_access_iterator;
    };

    template<typename T>
    struct Span
    {
        std::span<const T> span;

    public:
        Span(const std::initializer_list<T>& init)
            : span(init.begin(), init.end())
        {}

        template<class C>
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
        Span& operator=(const Span& other) noexcept { span = other.span; }

        decltype(auto) begin() const noexcept { return span.data(); }
        decltype(auto) end()   const noexcept { return span.data() + span.size(); }

        const T* data() const noexcept { return span.data(); }
        usz size() const noexcept { return span.size(); }
    };

// -----------------------------------------------------------------------------
//                        Type erased functor reference
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
//                          Type erased Byte-view
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
//                         Type erased handle
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

        bool operator==(Handle other) const noexcept { return impl == other.impl; }
    };

// -----------------------------------------------------------------------------
//                             Index free list
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
//                               Hash Map/Set
// -----------------------------------------------------------------------------

    template<typename K, typename V>
    using HashMap = ankerl::unordered_dense::map<K, V>;

    template<typename E>
    using HashSet = ankerl::unordered_dense::set<E>;
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