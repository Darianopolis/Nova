#pragma once

#include <nova/core/nova_Core.hpp>

namespace nova
{
    namespace vfs
    {
        std::optional<Span<const b8>> LoadMaybe(StringView path);
        Span<const b8> Load(StringView path);

        std::optional<StringView> LoadStringMaybe(StringView path);
        StringView LoadString(StringView path);

        void ForEach(std::function<void(StringView, Span<const b8>)> for_each);
    }
}