#pragma once

#include "Core.hpp"

// #define PYR_NOISY_REFS 1
// #define PYR_NOISY_REF_INC 1
// #define PYR_NOISY_REF_CONSTRUCT 1

#define PYR_SAFE_REFERENCES 1

int32_t& RefTotal();

namespace pyr
{
    class RefCounted
    {
        template<class T>
        friend class Ref;

        static size_t& RefTotal()
        {
            static size_t refTotal = 0;
            return refTotal;
        }
    private:
        std::atomic<u32> referenceCount = 0;

    public:
#ifdef PYR_NOISY_REFS
        RefCounted()
        {
            PYR_LOG("RefCounted  + {}", ++RefTotal());
        }

        ~RefCounted()
        {
            PYR_LOG("RefCounted -  {}", --RefTotal());
        }
#else
        RefCounted() = default;
        virtual ~RefCounted() {}
#endif

        // RefCounted objects are STRICTLY unique and pointer stable
        RefCounted(RefCounted&&) = delete;
        RefCounted(const RefCounted&) = delete;
        RefCounted& operator=(RefCounted&&) = delete;
        RefCounted& operator=(const RefCounted&) = delete;
    };

    template<class T>
    class Ref
    {
    private:

        T* value{};

        inline void Decrement()
        {
#ifdef PYR_NOISY_REF_INC
            PYR_LOG("decrementing {}[{}] from {}", typeid(T).name(), (void*)value,  value->referenceCount.load());
#endif
            if (!--static_cast<RefCounted*>(value)->referenceCount) {
#ifdef PYR_SAFE_REFERENCES
                static_cast<RefCounted*>(value)->referenceCount = std::numeric_limits<u32>::max();
#endif
                delete value;
            }
        }

        inline void Increment()
        {
#ifdef PYR_NOISY_REF_INC
            PYR_LOG("incrementing {}[{}] to {}", typeid(T).name(), (void*)value, (value->referenceCount.load() + 1));
#endif
#ifdef PYR_SAFE_REFERENCES
            if (static_cast<RefCounted*>(value)->referenceCount == std::numeric_limits<u32>::max()) [[unlikely]] {
                PYR_THROW("Attempted to increment Ref on object that is being destroyed!");
            }
#endif
            ++static_cast<RefCounted*>(value)->referenceCount;
        }

        inline b8 GuardedAssignment(T* other)
        {
            if (value) {
                if (value == other) return false;
                Decrement();
            }
            return (value = other);
        }
    public:

// ----------------  Default Construction and Destruction -------------------- //

        inline Ref()
            : value(nullptr)
        {

        }

        inline ~Ref()
        {
            if (value) Decrement();
        }

//  ----------------- Pointer construction and assignment ------------------- //

        inline Ref(T* value)
            : value(value)
        {
            if (value) Increment();
        }

        inline Ref<T>& operator=(T* other)
        {
            if (GuardedAssignment(other))
                Increment();

            return *this;
        }

// -------------------- Move construction and assignment -------------------- //

        inline Ref(Ref<T>&& moved
#ifdef PYR_NOISY_REF_CONSTRUCT
            , std::source_location location = std::source_location::current()
#endif
        )
#if !(defined(PYR_SAFE_REFERENCES) || defined(PYR_NOISY_REF_CONSTRUCT))
                                   noexcept
#endif
            : value(moved.value)
        {
#ifdef PYR_SAFE_REFERENCES
            if (this == &moved)
                PYR_THROW("Ref::Ref(Ref<T>&&) called on self");
#endif
#ifdef PYR_NOISY_REF_CONSTRUCT
            PYR_LOG("Ref::Ref({}&&) at {} @ {}", typeid(T).name(), location.line(), location.file_name());
#endif
            moved.value = nullptr;
        }

        inline Ref<T>& operator=(Ref<T>&& moved) noexcept
        {
            if (GuardedAssignment(moved.value))
                moved.value = nullptr;

            return *this;
        }

// -------------------- Copy construction and assignment --------------------- //

        inline Ref(const Ref<T>& copied
#ifdef PYR_NOISY_REF_CONSTRUCT
            , std::source_location location = std::source_location::current()
#endif
        )
            : value(copied.value)
        {
#ifdef PYR_SAFE_REFERENCES
            if (this == &copied)
                PYR_THROW("Ref::Ref(const Ref<T>&) called on self");
#endif
#ifdef PYR_NOISY_REF_CONSTRUCT
            PYR_LOG("Ref::Ref(const {}&) at {} @ {}", typeid(T).name(), location.line(), location.file_name());
#endif
            if (value) Increment();
        }

        inline Ref<T>& operator=(const Ref<T>& copied)
        {
            if (GuardedAssignment(copied.value))
                Increment();

            return *this;
        }

// --------------------------------- Query ---------------------------------- //

        // Disable all implicit casts except those defined here
        template<class T>
        inline operator T() const = delete;

        inline operator b8() const { return value; }

//--------------------------------- Upcast --------------------------------- //

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

// -------------------------------- Compare --------------------------------- //

        auto operator==(const Ref<T>& other) const
        {
            return value == other.value;
        }

// --------------------------------- Query ---------------------------------- //

        template<class T2>
        Ref<T2> As() const
        {
#ifdef PYR_SAFE_REFERENCES
            auto cast = dynamic_cast<T2*>(value);
            if (value && !cast)
                PYR_THROW("Invalid cast!");
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

// ------------------------------- Accessors -------------------------------- //

        inline const T* operator->() const
        {
#ifdef PYR_SAFE_REFERENCES
            if (!value)
                PYR_THROW("Ref<{}>::operator-> called on null reference", typeid(T).name());
#endif
            return value;
        }
        inline T* operator->()
        {
#ifdef PYR_SAFE_REFERENCES
            if (!value)
                PYR_THROW("Ref<{}>::operator-> called on null reference", typeid(T).name());
#endif
            return value;
        }

        inline const T& operator*() const
        {
#ifdef PYR_SAFE_REFERENCES
            if (!value)
                PYR_THROW("Ref<{}>::operator* called on null reference", typeid(T).name());
#endif
            return *value;
        }

        inline T& operator*()
        {
#ifdef PYR_SAFE_REFERENCES
            if (!value)
                PYR_THROW("Ref<{}>::operator* called on null reference", typeid(T).name());
#endif
            return *value;
        }

        inline const T* Raw() const { return value; }
        inline T* Raw() { return value; }
    };
}
