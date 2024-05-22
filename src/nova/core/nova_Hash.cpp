#include "nova_Hash.hpp"

namespace nova::hash
{
    u64 Hash(const void* data, usz size)
    {
        return ankerl::unordered_dense::detail::wyhash::hash(data, size);
    }

    u64 Mix(u64 a, u64 b)
    {
        return ankerl::unordered_dense::detail::wyhash::mix(a, b);
    }
}