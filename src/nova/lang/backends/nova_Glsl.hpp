#pragma once

#include <nova/lang/nova_Lang.hpp>

namespace nova
{
    struct GlslGenerator
    {
        Parser* parser = nullptr;
        Resolver* resolver = nullptr;

    public:
        void Generate(std::ostream& out);
    };
}