#include "nova_Core.hpp"

namespace nova
{
    template<typename T, usz InlineSize>
    class Array
    {
        u64 _size  : 63 = 0;
        u64 _is_large : 1  = 0;

        union
        {
            struct ArrayDynData
            {
                T* data;
                usz capacity;
            } _large;
            T[InlineSize] _small;
        };

        void EnsureCapacity(usz _new_size)
        {
            usz cur_capacity = capacity();

            // Only resize if necessary
            if (cur_capacity >= _new_size)
                return;

            usz _large.capacity = usz(cur_capacity * 1.5);

            // If resizing we will always be large now
            T* new_data = static_cast<T*>(mi_malloc(_large.capacity * sizeof(T)));

            // Copy and free iff was previously large
            // Don't call constructors/destructors, trivially relocatable requiremnt!
            std::memcpy(new_data, data(), _size * sizeof(T));
            if (_is_large)
                mi_free(_large.data);

            _large.data = new_data;
            _is_large = 1;
        }

    public:
        T* data() noexcept
        {
            return _is_large ? _large.data : _small;
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
            new(loc) T(std::forward<Args>args...);
            return *loc;
        }
    };
}