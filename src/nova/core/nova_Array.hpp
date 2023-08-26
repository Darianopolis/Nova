#include "nova_Core.hpp"

namespace nova
{
    template<typename T, usz InlineSize>
    class Array
    {
        u64 _size     : 63 = 0;
        u64 _is_large : 1  = 0;

        union
        {
            struct ArrayDynData
            {
                T* data;
                usz capacity;
            } _large;
            T _small[InlineSize];
        };

        void EnsureCapacity(usz _new_size)
        {
            usz cur_capacity = capacity();

            // Only resize if necessary
            if (cur_capacity >= _new_size) {
                return;
            }

            _large.capacity = usz(cur_capacity * 1.5);

            // If resizing we will always be large now
            T* new_data = static_cast<T*>(mi_malloc(_large.capacity * sizeof(T)));

            // Copy and free iff was previously large
            // Don't call constructors/destructors, trivially relocatable requiremnt!
            std::memcpy(new_data, data(), _size * sizeof(T));
            if (_is_large) {
                mi_free(_large.data);
            }

            _large.data = new_data;
            _is_large = 1;
        }

    public:
        Array() {}

        ~Array()
        {
            clear();
            if (_is_large) {
                mi_free(_large.data);
            }
        }

    public:
        void clear() noexcept(noexcept(std::declval<T>().~T()))
        {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (auto& e : *this) {
                    e.~T();
                }
            }

            _size = 0;
        }

        T* data() noexcept
        {
            return _is_large ? _large.data : _small;
        }

        const T* data() const noexcept
        {
            return _is_large ? _large.data : _small;
        }

        T& operator[](usz index) noexcept
        {
            return data()[index];
        }

        const T& operator[](usz index) const noexcept
        {
            return data()[index];
        }

        usz size() const noexcept
        {
            return _size;
        }

        usz capacity() const noexcept
        {
            return _is_large ? _large.capacity : InlineSize;
        }

        template<typename ... Args>
        T& emplace_back(Args&& ... args)
        {
            EnsureCapacity(_size + 1);
            T* loc = data() + _size++;
            new(loc) T(std::forward<Args>(args)...);
            return *loc;
        }

        void push_back(const T& t)
        {
            emplace_back(t);
        }

        void push_back(T&& t)
        {
            emplace_back(std::move(t));
        }

    public:
        T* begin() noexcept
        {
            return data();
        }

        T* end() noexcept
        {
            return data() + _size;
        }

        const T* begin() const noexcept
        {
            return data();
        }

        const T* end() const noexcept
        {
            return data() + _size;
        }

    public:
        T& back()
        {
            return data() + _size - 1;
        }

        void pop_back()
        {
            if constexpr (!std::is_trivially_destructible_v<T>()) {
                (data() + --_size)->~T();
            }
            else {
                --_size;
            }
        }
    };
}