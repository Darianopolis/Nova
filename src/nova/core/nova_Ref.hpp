#pragma once

#include <nova/core/nova_Core.hpp>

// #define NOVA_NOISY_REFS 1
// #define NOVA_NOISY_REF_INC 1
// #define NOVA_NOISY_REF_CONSTRUCT 1

#define NOVA_SAFE_REFERENCES 1

int32_t& RefTotal();

namespace nova::types
{
// -----------------------------------------------------------------------------
//                          Reference counted base
// -----------------------------------------------------------------------------

    class RefCounted
    {
        static size_t& RefTotal()
        {
            static size_t refTotal = 0;
            return refTotal;
        }
    private:
        u32 referenceCount = 0;
        static constexpr u32 InvalidRefCount = ~0u;

    public:
        RefCounted() = default;
        virtual ~RefCounted() {}

        void Acquire()
        {
#ifdef NOVA_SAFE_REFERENCES
            if (std::atomic_ref<u32>(referenceCount) == InvalidRefCount) [[unlikely]] {
                NOVA_THROW("Attempted to Acquire on RefCounted object that is being destroyed!");
            }
#endif
            ++std::atomic_ref<u32>(referenceCount);
        }

        bool Release()
        {
            bool toDelete = !--std::atomic_ref<u32>(referenceCount);
#ifdef NOVA_SAFE_REFERENCES
            if (toDelete)
                std::atomic_ref<u32>(referenceCount).store(InvalidRefCount);
#endif
            return toDelete;
        }

        // RefCounted objects are STRICTLY unique and pointer stable
        RefCounted(RefCounted&&) = delete;
        RefCounted(const RefCounted&) = delete;
        RefCounted& operator=(RefCounted&&) = delete;
        RefCounted& operator=(const RefCounted&) = delete;
    };

// -----------------------------------------------------------------------------
//                          Reference counted pointer
// -----------------------------------------------------------------------------

    template<class T>
    class Ref
    {
        T* value{};

    public:

// -----------------------------------------------------------------------------

        Ref()
            : value(nullptr)
        {

        }

        ~Ref()
        {
            if (value)
                value->Release();
        }

// -----------------------------------------------------------------------------

        template<class... Args>
        static Ref<T> Create(Args&&... args)
        {
            return Ref(new T(std::forward<Args>(args)...));
        }

// -----------------------------------------------------------------------------

        Ref(T* value)
            : value(value)
        {
            if (value)
                value->Acquire();
        }

// -----------------------------------------------------------------------------

        inline Ref(Ref<T>&& moved)
            : value(moved.value)
        {
#ifdef NOVA_SAFE_REFERENCES
            if (this == &moved)
                NOVA_THROW("Ref::Ref(Ref<T>&&) called on self");
#endif
            moved.value = nullptr;
        }

        inline Ref<T>& operator=(Ref<T>&& moved) noexcept
        {
            if (value != moved.value)
            {
                this->~Ref();
                new (this) Ref(moved);
            }
            return *this;
        }

// -----------------------------------------------------------------------------

        inline Ref(const Ref<T>& copied)
            : value(copied.value)
        {
#ifdef NOVA_SAFE_REFERENCES
            if (this == &copied)
                NOVA_THROW("Ref::Ref(const Ref<T>&) called on self");
#endif
            if (value)
                value->Acquire();
        }

        inline Ref<T>& operator=(const Ref<T>& copied)
        {
            if (value != copied.value)
            {
                this->~Ref();
                new (this) Ref(copied);
            }
            return *this;
        }

// -----------------------------------------------------------------------------

        bool HasValue() const noexcept
        {
            return value;
        }

        auto operator==(const Ref<T>& other) const
        {
            return value == other.value;
        }

// -----------------------------------------------------------------------------

        operator T&() &
        {
            return *value;
        }

        operator T*() &
        {
            return value;
        }

// -----------------------------------------------------------------------------
//                                 Upcasting
// -----------------------------------------------------------------------------

        template<class T2>
        requires std::derived_from<T, T2>
        operator Ref<T2>() const
        {
            return Ref<T2>(value);
        }

        template<class T2>
        requires std::derived_from<T, T2>
        Ref<T2> ToBase()
        {
            return Ref<T2>(value);
        }

// -----------------------------------------------------------------------------
//                                Downcasting
// -----------------------------------------------------------------------------

        template<class T2>
        Ref<T2> As() const
        {
#ifdef NOVA_SAFE_REFERENCES
            auto cast = dynamic_cast<T2*>(value);
            if (value && !cast)
                NOVA_THROW("Invalid cast!");
            return Ref<T2>(cast);
#else
            return Ref<T2>(static_cast<T2*>(value));
#endif
        }

        template<class T2>
        Ref<T2> TryAs() const
        {
            return Ref<T2>(dynamic_cast<T2*>(value));
        }

// -----------------------------------------------------------------------------
//                                Accessors
// -----------------------------------------------------------------------------

        inline const T* operator->() const
        {
#ifdef NOVA_SAFE_REFERENCES
            if (!value)
                NOVA_THROW("Ref<{}>::operator-> called on null reference", typeid(T).name());
#endif
            return value;
        }
        inline T* operator->()
        {
#ifdef NOVA_SAFE_REFERENCES
            if (!value)
                NOVA_THROW("Ref<{}>::operator-> called on null reference", typeid(T).name());
#endif
            return value;
        }

        inline const T& operator*() const
        {
#ifdef NOVA_SAFE_REFERENCES
            if (!value)
                NOVA_THROW("Ref<{}>::operator* called on null reference", typeid(T).name());
#endif
            return *value;
        }

        inline T& operator*()
        {
#ifdef NOVA_SAFE_REFERENCES
            if (!value)
                NOVA_THROW("Ref<{}>::operator* called on null reference", typeid(T).name());
#endif
            return *value;
        }

        inline const T* Raw() const { return value; }
        inline T* Raw() { return value; }
    };
}