#include "nova_Allocation.hpp"

#include <mimalloc.h>

namespace nova
{
    void* Alloc(usz size)
    {
        return mi_malloc(size);
    }

    void* Alloc(usz size, usz align)
    {
        return mi_malloc_aligned(size, align);
    }

    void Free(void* ptr)
    {
        mi_free(ptr);
    }
}