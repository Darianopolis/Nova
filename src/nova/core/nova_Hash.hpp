#pragma once

#include "nova_Core.hpp"

#include <ankerl/unordered_dense.h>

#define NOVA_MEMORY_EQUALITY_MEMBER(type)                                      \
    bool operator==(const type& other) const                                   \
    {                                                                          \
        return std::memcmp(this, &other, sizeof(type)) == 0;                   \
    }

#define NOVA_MEMORY_HASH(type)                                                 \
    template<>                                                                 \
    struct ankerl::unordered_dense::hash<type>                                 \
    {                                                                          \
        using is_avalanching = void;                                           \
        uint64_t operator()(const type& key) const noexcept {                  \
            return detail::wyhash::hash(&key, sizeof(key));                    \
        }                                                                      \
    };