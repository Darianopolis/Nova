#pragma once

#include "nova_Core.hpp"

namespace nova
{
    struct IBase
    {
    private:
        std::atomic_uint32_t referenceCount = 0;

    public:
        virtual ~IBase() {};

        void Acquire()
        {
            NOVA_LOG("Acquiring, {} -> {}", referenceCount.load(), referenceCount.load() + 1);
            referenceCount++;
        }

        void Release()
        {
            NOVA_LOG("Releasing, {} -> {}", referenceCount.load(), referenceCount.load() - 1);
            if (--referenceCount == 0)
            {
                NOVA_LOG("Deleting this!");
                delete this;
            }
        }
    };

    template<class T>
    class Ptr
    {
        T* t = nullptr;

    public:
        Ptr() = default;

        Ptr(T* _t)
            : t(_t)
        {
            t->Acquire();
        }

        Ptr(const Ptr& other)
            : t(other.t)
        {
            if (t) t->Acquire();
        }

        Ptr& operator=(const Ptr& other)
        {
            if (t != other.t)
            {
                if (t) t->Release();
                t = other.t;
                if (t) t->Acquire();
            }

            return *this;
        }

        Ptr(Ptr&& other)
            : t(other.t)
        {
            other.t = nullptr;
        }

        Ptr& operator=(Ptr&& other)
        {
            if (t != other.t)
            {
                if (t) t->Release();
                t = other.t;
                other.t = nullptr;
            }

            return *this;
        }

        ~Ptr()
        {
            NOVA_LOG("Destroying, t = {}", (void*)t);
            if (t)
                t->Release();
        }

        operator T*()
        {
            return t;
        }

        T* operator->()
        {
            return t;
        }

        T& operator*()
        {
            return *t;
        }
    };
}