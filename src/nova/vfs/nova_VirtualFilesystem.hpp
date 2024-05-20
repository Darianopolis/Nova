#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Containers.hpp>
#include <nova/core/nova_Strings.hpp>

namespace nova
{
    namespace vfs
    {
        namespace detail
        {
            std::monostate Register(std::string path, Span<const b8> data);
            std::monostate RegisterUC8(std::string path, Span<const uc8> data);
        }

        template<usz Size>
        Span<const b8> FromString(const char (&str)[Size])
        {
            return Span((const b8*)&str[0], Size);
        }

        std::optional<Span<const b8>> LoadMaybe(StringView path);
        Span<const b8> Load(StringView path);

        std::optional<StringView> LoadStringMaybe(StringView path);
        StringView LoadString(StringView path);

        void ForEach(std::function<void(StringView, Span<const b8>)> for_each);
    }
}