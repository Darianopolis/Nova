#include "nova_VirtualFilesystem.hpp"

namespace nova::vfs
{
    namespace detail
    {
        struct VirtualFilesystem
        {
            std::vector<std::unique_ptr<std::string>>                      paths;
            ankerl::unordered_dense::map<std::string_view, Span<const b8>> files;
        };

        static VirtualFilesystem NovaVFS;

        std::monostate Register(std::string path, Span<const b8> data)
        {
            std::string_view str = *NovaVFS.paths.emplace_back(new std::string(std::move(path)));
            NovaVFS.files[str] = data;

            return {};
        }

        std::monostate RegisterUC8(std::string path, Span<const uc8> data)
        {
            return Register(std::move(path), Span<const b8>((const b8*)data.data(), data.size()));
        }
    }

    std::optional<Span<const b8>> LoadMaybe(StringView path)
    {
        auto& vfs = detail::NovaVFS;
        auto i = vfs.files.find(std::string_view(path));
        if (i == vfs.files.end()) {
            return std::nullopt;
        }

        return i->second;
    }

    Span<const b8> Load(StringView path)
    {
        auto opt = LoadMaybe(path);
        if (opt) {
            return opt.value();
        }

        NOVA_THROW_STACKLESS("File [{}] not found in Nova VFS", path);
    }

    std::optional<StringView> LoadStringMaybe(StringView path)
    {
        auto data = LoadMaybe(path);
        if (data) return StringView((const char*)data->data(), data->size());
        return std::nullopt;
    }

    StringView LoadString(StringView path)
    {
        auto data = Load(path);
        return StringView((const char*)data.data(), data.size());
    }

    void ForEach(std::function<void(StringView, Span<const b8>)> for_each)
    {
        for (auto[path, contents] : detail::NovaVFS.files) {
            for_each(StringView(path), contents);
        }
    }
}