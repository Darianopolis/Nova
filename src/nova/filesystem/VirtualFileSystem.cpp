#include "VirtualFileSystem.hpp"

#include <nova/core/Files.hpp>

namespace nova::vfs
{
    namespace detail
    {
        struct VirtualFilesystem
        {
            std::vector<std::unique_ptr<std::string>>                      paths;
            ankerl::unordered_dense::map<std::string_view, Span<const b8>> files;

            void Register(std::string name, const void* data, size_t size)
            {
                std::string_view str = *paths.emplace_back(new std::string(std::move(name)));
                files[str] = Span((const b8*)data, size);
            }
        };

        VirtualFilesystem& GetVFS()
        {
            static VirtualFilesystem vfs;
            return vfs;
        }

        int Register(const char* name, const void* data, size_t size)
        {
            GetVFS().Register(name, data, size);
            return {};
        }
    }

    std::optional<Span<const b8>> LoadMaybe(StringView path)
    {
        auto& vfs = detail::GetVFS();
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

        NOVA_THROW("File [{}] not found in Nova VFS", path);
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
        for (auto[path, contents] : detail::GetVFS().files) {
            for_each(StringView(path), contents);
        }
    }
}

void bldr_register_embed(const char* name, const void* code, size_t size_in_bytes)
{
    nova::vfs::detail::Register(name, code, size_in_bytes);
}
