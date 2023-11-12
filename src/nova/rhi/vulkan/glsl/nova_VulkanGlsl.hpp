#pragma once

#include <nova/rhi/nova_RHI.hpp>

namespace nova
{
    namespace glsl
    {
        std::vector<uint32_t> Compile(
            ShaderStage                stage,
            std::string_view           entry,
            const std::string&      filename,
            Span<std::string_view> fragments = {});
    }
}