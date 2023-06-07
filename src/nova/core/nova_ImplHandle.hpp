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

    template<class TImpl>
    class ImplHandle
    {
    protected:
        TImpl* impl = {};

    public:
        ImplHandle() = default;

        ImplHandle(TImpl* _impl) noexcept
            : impl(_impl)
        {
            if (impl) impl->Acquire();
        }

        ~ImplHandle() noexcept
        {
            if (impl && impl->Release()) delete impl;
        }

// -----------------------------------------------------------------------------

        ImplHandle(const ImplHandle& other) noexcept
            : impl(other.impl)
        {
            if (impl) impl->Acquire();
        }

        ImplHandle& operator=(const ImplHandle& other) noexcept
        {
            if (impl != other.impl)
            {
                this->~ImplHandle();
                new (this) ImplHandle(other);
            }
            return *this;
        }

        ImplHandle(ImplHandle&& other) noexcept
            : impl(other.impl)
        {
            other.impl = nullptr;
        }

        ImplHandle& operator=(ImplHandle&& other) noexcept
        {
            if (impl != other.impl)
            {
                this->~ImplHandle();
                new (this) ImplHandle(std::move(other));
            }
            return *this;
        }

// -----------------------------------------------------------------------------

        bool operator==(const ImplHandle& other) const noexcept
        {
            return impl == other.impl;
        }

// -----------------------------------------------------------------------------

        bool IsValid() const noexcept { return impl; }

        void SetImpl(TImpl* _impl) noexcept {
            if (impl != _impl)
            {
                if (impl && impl->Release()) delete impl;
                impl = _impl;
                if (impl) impl->Acquire();
            }
        }

        TImpl* GetImpl() const noexcept { return impl; }

        operator TImpl*() const noexcept { return impl; }

        // TODO: Should this really be treated like a smart pointer externally?
        TImpl* operator->() const noexcept { return impl; };
    };

#define NOVA_DECLARE_HANDLE_OBJECT(type) \
    struct type;                         \
    struct type##Impl;                   \
    using H##type = type##Impl*;

#define NOVA_DECLARE_HANDLE_OPERATIONS(type)      \
    type() noexcept;                              \
    type(type##Impl* impl) noexcept;              \
    ~type();                                      \
    type(const type& other) noexcept;             \
    type& operator=(const type& other) noexcept;  \
    type(type&& other) noexcept;                  \
    type& operator=(type&& other) noexcept;

#define NOVA_DEFINE_HANDLE_OPERATIONS(type)                      \
    type::type() noexcept = default;                             \
    type::type(type##Impl* impl) noexcept : ImplHandle(impl) {}  \
    type::~type() = default;                                     \
    type::type(const type& other) noexcept = default;            \
    type& type::operator=(const type& other) noexcept = default; \
    type::type(type&& other) noexcept = default;                 \
    type& type::operator=(type&& other) noexcept = default;
}