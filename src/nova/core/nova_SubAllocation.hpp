#pragma once

#include <nova/core/nova_Core.hpp>

namespace nova
{
    struct IndexFreeList
    {
        u32             next_index = 0;
        std::vector<u32> free_list;

    public:
        inline
        u32 Acquire()
        {
            if (free_list.empty())
                return next_index++;

            u32 index = free_list.back();
            free_list.pop_back();
            return index;
        }

        inline
        void Release(u32 index)
        {
            free_list.push_back(index);
        }
    };
}