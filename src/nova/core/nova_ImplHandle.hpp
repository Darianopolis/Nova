#pragma once

#include "nova_Core.hpp"

namespace nova
{
    inline std::atomic_uint64_t NumHandleOperations = 0;

    class ImplBase
    {
        u32 referenceCount = 0;

    protected:
        ~ImplBase() {}

    public:
        ImplBase() = default;

        void Acquire()
        {
            ++NumHandleOperations;
            ++std::atomic_ref(referenceCount);
            // NOVA_LOG("Reference count  + {}", referenceCount);
        }

        bool Release()
        {
            ++NumHandleOperations;
            // NOVA_LOG("Reference count -  {}", referenceCount - 1);
            return !--std::atomic_ref(referenceCount);
        }

        u32 GetReferenceCount() const noexcept
        {
            return std::atomic_ref(referenceCount).load();
        }
    };

// -----------------------------------------------------------------------------
//                            Base Impl Handle
// -----------------------------------------------------------------------------

    template<class TImpl>
    class ImplHandleArc;

    template<class TImpl>
    class ImplHandle
    {
    public:
        using ImplType = TImpl;

    protected:
        TImpl* impl = {};

    public:
        ImplHandle() = default;

        ImplHandle(TImpl* _impl) noexcept
            : impl(_impl)
        {}

// -----------------------------------------------------------------------------

        bool operator==(const ImplHandle& other) const noexcept
        {
            return impl == other.impl;
        }

// -----------------------------------------------------------------------------

        bool IsValid() const noexcept { return impl; }

        void SetImpl(TImpl* _impl) noexcept { impl = _impl; }

        TImpl* GetImpl()    const noexcept { return impl; }
        TImpl* operator->() const noexcept { return impl; };

        ImplHandle operator-() const noexcept { return *this; }
    };

// -----------------------------------------------------------------------------
//                       Reference counted Impl Handle
// -----------------------------------------------------------------------------

    template<class TImplHandle>
    class ImplHandleArc : public TImplHandle
    {
    public:
        ImplHandleArc() = default;
        ImplHandleArc(TImplHandle::ImplType* _impl) noexcept;
        ~ImplHandleArc() noexcept;
        ImplHandleArc(const ImplHandleArc& other) noexcept;
        void SetImpl(TImplHandle::ImplType* impl) noexcept;

// -----------------------------------------------------------------------------

        ImplHandleArc& operator=(const ImplHandleArc& other) noexcept
        {
            if (this->impl != other.impl)
            {
                this->~ImplHandleArc();
                new (this) ImplHandleArc(other);
            }
            return *this;
        }

        ImplHandleArc(ImplHandleArc&& other) noexcept
            : TImplHandle(other.impl)
        {
            other.impl = nullptr;
        }

        ImplHandleArc& operator=(ImplHandleArc&& other) noexcept
        {
            if (this->impl != other.impl)
            {
                this->~ImplHandleArc();
                new (this) ImplHandleArc(std::move(other));
            }
            return *this;
        }

// -----------------------------------------------------------------------------

        bool operator==(const ImplHandleArc& other) const noexcept
        {
            return this->impl == other.impl;
        }

// -----------------------------------------------------------------------------

        TImplHandle operator-() const noexcept { return this->impl; }
    };

// -----------------------------------------------------------------------------
//                      Handle Declare/Define Helpers
// -----------------------------------------------------------------------------

#define NOVA_DECLARE_HANDLE_OBJECT(type) \
    struct type;                         \
    struct type##Impl;

#define NOVA_DECLARE_HANDLE_OPERATIONS(type) \
    type() noexcept;                         \
    type(type##Impl* impl) noexcept;         \
    using Arc = ImplHandleArc<type>;         \
    Arc operator+() const;

#define NOVA_DEFINE_HANDLE_OPERATIONS(type)                                   \
    type::type() noexcept = default;                                          \
    type::type(type##Impl* impl) noexcept : ImplHandle(impl) {}               \
    type::Arc type::operator+() const { return type::Arc(impl); }             \
    template<> ImplHandleArc<type>::ImplHandleArc(type##Impl* _impl) noexcept \
        : type(_impl)                                                         \
    {                                                                         \
        if (this->impl) this->impl->Acquire();                                \
    }                                                                         \
    template<> ImplHandleArc<type>::~ImplHandleArc() noexcept                 \
    {                                                                         \
        if (this->impl && this->impl->Release())                              \
            delete this->impl;                                                \
    }                                                                         \
    template<> ImplHandleArc<type>::ImplHandleArc(                            \
            const ImplHandleArc<type>& other) noexcept                        \
        : type(other.impl)                                                    \
    {                                                                         \
        if (this->impl) this->impl->Acquire();                                \
    }                                                                         \
    template<> void ImplHandleArc<type>::SetImpl(type##Impl* _impl) noexcept  \
    {                                                                         \
        if (this->impl != _impl)                                              \
        {                                                                     \
            if (this->impl && this->impl->Release()) delete this->impl;       \
            this->impl = _impl;                                               \
            if (this->impl) this->impl->Acquire();                            \
        }                                                                     \
    }
}
