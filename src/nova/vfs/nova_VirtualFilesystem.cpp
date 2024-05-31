#include "nova_VirtualFilesystem.hpp"

#include <nova/core/nova_Files.hpp>

namespace nova::vfs
{
    namespace detail
    {
        struct VirtualFilesystem
        {
            std::vector<std::unique_ptr<std::string>>                      paths;
            ankerl::unordered_dense::map<std::string_view, Span<const b8>> files;

            std::vector<MappedFile> mapped_files;

            void LoadAllPackFiles()
            {
                auto exe_dir = env::GetExecutablePath().parent_path();
                auto cur_dir = fs::current_path();
                LoadPackFilesIn(exe_dir);
                if (!fs::equivalent(exe_dir, cur_dir)) {
                    LoadPackFilesIn(cur_dir);
                }
            }

            void LoadPackFilesIn(std::filesystem::path dir)
            {
                nova::Log("looking for .npk files in [{}]", dir.string());

                for (auto& entry : fs::directory_iterator(dir)) {
                    if (!entry.is_regular_file()) continue;

                    auto path = entry.path();

                    auto ext = path.extension().string();
                    std::ranges::transform(ext, ext.data(), [](char c) { return char(std::toupper(c)); });

                    if (ext == ".NPK") {
                        LoadPackFile(path);
                    }
                }
            }

            void LoadPackFile(std::filesystem::path path)
            {
                nova::Log("Attempting to load npk file: {}", path.string());

                auto file = MappedFile::Open(path.string(), false);

                NOVA_DEBUGEXPR(file.GetSize());

                auto base_address = file.GetAddress();
                NOVA_DEBUGEXPR(base_address);

                if (file.Read<std::remove_cvref_t<decltype(PackFileMagic)>>() != PackFileMagic) {
                    nova::Log(".npk file with non matching magic, skipping...");
                    return;
                }

                auto version = file.Read<u64>();
                NOVA_DEBUGEXPR(version);

                auto header_offset = file.GetOffset();
                auto header_size = file.Read<u64>();
                auto data_offset = header_offset + header_size;

                auto entry_count = file.Read<u64>();
                NOVA_DEBUGEXPR(entry_count);

                NOVA_DEBUGEXPR(header_offset);
                NOVA_DEBUGEXPR(header_size);
                NOVA_DEBUGEXPR(data_offset);

                for (u64 i = 0; i < entry_count; ++i) {
                    auto path_len = file.Read<u64>();
                    std::string str(path_len, '\0');
                    file.Read(str.data(), path_len);

                    auto offset = file.Read<u64>();
                    auto size = file.Read<u64>();

                    // nova::Log("Loaded npk resource (path = {}, offset = {}, size = {})", str, offset, size);
                    Register(std::move(str), ByteOffsetPointer(base_address, offset + data_offset), size);
                }
            }

            void Register(std::string name, const void* data, size_t size)
            {
                std::string_view str = *paths.emplace_back(new std::string(std::move(name)));
                files[str] = Span((const b8*)data, size);
            }

            VirtualFilesystem()
            {
                auto start = chr::steady_clock::now();

                LoadAllPackFiles();

                nova::Log("Initialized VFS in {}", DurationToString(chr::steady_clock::now() - start));
            }

            ~VirtualFilesystem()
            {
                nova::Log("Shutting down virtual filesystem, closing pack files");
                for (auto& pack : mapped_files) {
                    pack.Destroy();
                }
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