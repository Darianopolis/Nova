#pragma once

#include <nova/core/Core.hpp>

namespace nova
{
    namespace vfs
    {
        constexpr static std::array<char, 8> PackFileMagic = { 'n', 'o', 'v', 'a', 'p', 'a', 'c', 'k' };
        constexpr static std::string_view PackFileExt = ".npk";

        namespace detail
        {
            int Register(const char* name, const void* data, size_t size);
        }

        std::optional<Span<const b8>> LoadMaybe(StringView path);
        Span<const b8> Load(StringView path);

        std::optional<StringView> LoadStringMaybe(StringView path);
        StringView LoadString(StringView path);

        void ForEach(std::function<void(StringView, Span<const b8>)> for_each);
    }
}