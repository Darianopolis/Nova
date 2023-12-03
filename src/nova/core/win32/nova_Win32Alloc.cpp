#include <nova/core/nova_Allocation.hpp>

#include <nova/core/win32/nova_Win32Include.hpp>

namespace nova
{
    void* AllocVirtual(AllocationType type, usz size)
    {
        DWORD win_type = {};
        if (type >= AllocationType::Commit)  win_type |= MEM_COMMIT;
        if (type >= AllocationType::Reserve) win_type |= MEM_RESERVE;
        return VirtualAlloc(nullptr, size, win_type, PAGE_READWRITE);
    }

    void FreeVirtual(FreeType type, void* ptr, usz size)
    {
        DWORD win_type = {};
        if (type >= FreeType::Decommit) win_type |= MEM_DECOMMIT;
        if (type >= FreeType::Release)  win_type |= MEM_RELEASE;
        VirtualFree(ptr, size, win_type);
    }
}