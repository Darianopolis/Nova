#include "nova_Stack.hpp"

#include "nova_Allocation.hpp"

namespace nova
{
    namespace detail
    {
        // TODO: Create allocation platform layer

        ThreadStack::ThreadStack()
            : ptr((std::byte*)AllocVirtual(AllocationType::Commit | AllocationType::Reserve, NOVA_STACK_SIZE))
        {}

        ThreadStack::~ThreadStack()
        {
            FreeVirtual(FreeType::Decommit, ptr);
        }

        ThreadStackPoint::ThreadStackPoint() noexcept
            : ptr(GetThreadStack().ptr)
        {}

        ThreadStackPoint::~ThreadStackPoint() noexcept
        {
            GetThreadStack().ptr = ptr;
        }
    }
}