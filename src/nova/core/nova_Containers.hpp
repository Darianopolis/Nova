#pragma once

#include "nova_Core.hpp"
#include "nova_Hash.hpp"

#include <ankerl/unordered_dense.h>

#include <ranges>

namespace nova
{
// -----------------------------------------------------------------------------
//                                  Span
// -----------------------------------------------------------------------------

    template<typename T>
    concept IsContiguousContainer = requires(T t)
    {
        { t.begin() } -> std::contiguous_iterator;
        { t.end() }   -> std::contiguous_iterator;
    };

    template<typename T>
    struct Span
    {
        std::span<const T> span;

    public:
        Span() = default;

        Span(const std::initializer_list<T>& init)
            : span(init.begin(), init.end())
        {}

        template<typename C>
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
        Span& operator=(const Span& other) noexcept { span = other.span; return *this; }

        decltype(auto) begin() const noexcept { return span.data(); }
        decltype(auto) end()   const noexcept { return span.data() + span.size(); }

        const T* data() const noexcept { return span.data(); }
        usz size() const noexcept { return span.size(); }
        bool empty() const noexcept { return span.empty(); }
    };

// -----------------------------------------------------------------------------
//                        Type erased functor reference
// -----------------------------------------------------------------------------

    template<typename Ret, typename... Types>
    struct FuncBase {
        void* body;
        Ret(*fptr)(void*, Types...);

        Ret operator()(Types... args) {
            return fptr(body, std::move<Types>(args)...);
        }
    };

    template<typename Tx>
    struct GetFunctionImpl {};

    template<typename Ret, typename... Types>
    struct GetFunctionImpl<Ret(Types...)> { using type = FuncBase<Ret, Types...>; };

    template<typename Sig>
    struct FunctionRef : GetFunctionImpl<Sig>::type {
        template<typename Fn>
        FunctionRef(Fn&& fn)
            : GetFunctionImpl<Sig>::type(&fn,
                [](void*b, auto&&... args) -> auto {
                    return (*(Fn*)b)(std::forward<decltype(args)>(args)...);
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

        template<typename T>
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

// -----------------------------------------------------------------------------

namespace nova
{
    inline constexpr const auto& Enumerate = std::ranges::views::enumerate;
}