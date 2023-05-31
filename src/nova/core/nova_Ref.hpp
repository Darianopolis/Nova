#pragma once

#include "nova_Core.hpp"

namespace nova
{
    struct RefCounted
    {
        std::atomic_uint32_t referenceCount = 0;

    public:
        virtual ~RefCounted() {};

        void Acquire()
        {
            // std::cout << '\n' << std::stacktrace::current() << '\n';
            // NOVA_LOGEXPR(this);
            NOVA_LOG("Acquiring, {} -> {}", referenceCount.load(), referenceCount.load() + 1);
            referenceCount++;
        }

        void Release()
        {
            // std::cout << '\n' << std::stacktrace::current() << '\n';
            // NOVA_LOGEXPR(this);
            NOVA_LOG("Releasing, {} -> {}", referenceCount.load(), referenceCount.load() - 1);
            if (--referenceCount == 0)
            {
                NOVA_LOG("Deleting this!");
                delete this;
            }
        }
    };

    namespace types
    {
        template<class T>
        class Rc
        {
            T* t = nullptr;

        public:
            Rc() = default;

            Rc(T* _t)
                : t(_t)
            {
                if (t) t->Acquire();
            }

            Rc(const Rc& other)
                : t(other.t)
            {
                if (t) t->Acquire();
            }

            Rc& operator=(const Rc& other)
            {
                if (t != other.t)
                {
                    if (t) t->Release();
                    t = other.t;
                    if (t) t->Acquire();
                }

                return *this;
            }

            Rc(Rc&& other)
                : t(other.t)
            {
                other.t = nullptr;
            }

            Rc& operator=(Rc&& other)
            {
                if (t != other.t)
                {
                    if (t) t->Release();
                    t = other.t;
                    other.t = nullptr;
                }

                return *this;
            }

            ~Rc()
            {
                NOVA_LOG("Destroying, t = {}", (void*)t);
                if (t)
                    t->Release();
            }

            operator T*() &
            {
                return t;
            }

            T* Get()
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
}