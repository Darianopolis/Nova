#include <nova/core/nova_Core.hpp>

namespace nova
{
    struct IndexFreeList
    {
        u32             nextIndex = 0;
        std::vector<u32> freeList;

    public:
        inline
        u32 Acquire()
        {
            if (freeList.empty())
                return nextIndex++;

            u32 index = freeList.back();
            freeList.pop_back();
            return index;
        }

        inline
        void Release(u32 index)
        {
            freeList.push_back(index);
        }
    };
}