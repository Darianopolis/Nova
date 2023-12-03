#pragma once

#include "nova_Core.hpp"
#include "nova_Flags.hpp"

namespace nova
{
    enum class AllocationType
    {
        Commit  = 1 << 0,
        Reserve = 1 << 1,
    };
    NOVA_DECORATE_FLAG_ENUM(AllocationType)

    enum class FreeType
    {
        Decommit = 1 << 0,
        Release  = 1 << 1,
    };
    NOVA_DECORATE_FLAG_ENUM(FreeType)

    void* AllocVirtual(AllocationType type, usz size);
    void FreeVirtual(FreeType type, void* ptr, usz size = 0);
}