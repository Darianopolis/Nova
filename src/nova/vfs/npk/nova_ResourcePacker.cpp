#include <nova/vfs/nova_VirtualFilesystem.hpp>

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Files.hpp>
#include <nova/core/nova_Base64.hpp>

#include <nova/rhi/slang/nova_SlangCompiler.hpp>

namespace nova
{
    struct EmbedFile
    {
        std::stringstream output;
        u32         resource_idx = 0;

        void AppendResource(StringView virtual_path, const void* data, size_t size_in_bytes)
        {
            auto u64_count = (size_in_bytes + 7) / 8;

            output << "// " << virtual_path << '\n';
            output << "namespace nova::vfs::detail { int Register(const char* name, const void* data, size_t size); }\n";

            u32 index = 0;
            constexpr u32 WrapIndex = 16;

            output << "static constexpr unsigned long long nova_vfs_resource" << resource_idx << "_array[] {\n";
            output << std::hex;
            for (u32 i = 0; i < u64_count; ++i) {
                usz offset = i * 8;
                u64 value = {};
                memcpy(&value, (const char*)data + offset, std::min(8ull, size_in_bytes - offset));
                output << "0x" << value << ",";
                if (index++ >= WrapIndex) {
                    output << '\n';
                    index = 0;
                }
            }
            output << std::dec;
            output << "};\n";
            output << "static int nova_vfs_resource" << resource_idx << " = nova::vfs::detail::Register(\"" << virtual_path << "\", nova_vfs_resource" << resource_idx << "_array, " << size_in_bytes << ");\n";

            resource_idx++;
        }
    };

    struct Packer
    {
        SlangCompiler              compiler;
        std::vector<StringView> search_dirs;

        std::string pack_output;

        static constexpr std::string_view              PackFolder = ".npk-embed";
        ankerl::unordered_dense::set<std::string> generated_files;

        bool embed = true;

        void Init(StringView output)
        {
            pack_output = Fmt("{}{}", output, vfs::PackFileExt);
            fs::create_directories(fs::path(pack_output).parent_path());
            fs::create_directories(PackFolder);
        }

        void Finalize()
        {
            if (embed) {
                fs::remove(pack_output);
            } else {
                SavePackFile();
            }
            RemoveStaleEmbeds();
        }

        struct Entry
        {
            std::string virtual_path;
            size_t            offset;
            size_t              size;
        };

        std::vector<Entry>  entries;
        std::vector<std::byte> data;

        void PadFileToPage(File& file)
        {
            constexpr static uint8_t zeroes[4096] = {};

            auto offset = file.GetOffset();
            auto aligned = AlignUpPower2(offset, 4096);
            if (aligned > offset) {
                file.Write(zeroes, aligned - offset);
            }
        }

        void RemoveStaleEmbeds()
        {
            for (auto& path : fs::directory_iterator(PackFolder)) {
                auto filename = path.path().filename().string();
                if (!generated_files.contains(filename)) {
                    // nova::Log("Removing pack file [{}]", filename);
                    fs::remove(path.path());
                }
            }
        }

        void SavePackFile()
        {
            File file(pack_output.c_str(), true);

            // Magic
            file.Write(vfs::PackFileMagic);

            // Version
            file.Write(0ull);

            // Header size placeholder (TODO calculate this up front)
            auto header_size_offset = file.GetOffset();
            file.Write(0ull);

            NOVA_DEBUGEXPR(header_size_offset);

            file.Write(entries.size());

            nova::Log("entry count = {}", entries.size());

            for (auto& entry : entries) {
                file.Write(entry.virtual_path.size());
                file.Write(entry.virtual_path.data(), entry.virtual_path.size());
                file.Write(entry.offset);
                file.Write(entry.size);
            }

            PadFileToPage(file);

            // Go back and poke file header size in
            // TODO: Need a more convenient way of doing this
            auto data_offset = file.GetOffset();
            file.Seek(header_size_offset);
            file.Write(data_offset - header_size_offset);
            file.Seek(data_offset);

            nova::Log("header size = {}", data_offset - header_size_offset);
            nova::Log("data offset = {}", data_offset);

            file.Write(data.data(), data.size());

            PadFileToPage(file);
        }

        usz SaveEmbedFile(const EmbedFile& pack_file)
        {
            auto new_contents = pack_file.output.str();

            auto hash = nova::hash::Hash(new_contents.data(), new_contents.size());
            auto output_file_name = Fmt("npk_Embed_s{}_h{}.cpp", new_contents.size(), hash);

            generated_files.emplace(output_file_name);

            auto path = fs::path(PackFolder) / output_file_name;

            if (fs::exists(path)) {
                auto prev_contents = files::ReadTextFile(path.string());
                if (prev_contents == new_contents) {
                    return 0;
                } else {
                    // TODO: Use discriminator to resolve collisions
                    NOVA_THROW("Hash collision!");
                }
            }

            std::ofstream file(path, std::ios::binary);
            file.write(new_contents.data(), new_contents.size());

            return new_contents.size();
        }

        void PushEntry(std::string virtual_path, const void* entry_data, size_t size)
        {
            if (embed) {
                EmbedFile embed_file;
                embed_file.AppendResource(virtual_path, entry_data, size);
                if (SaveEmbedFile(embed_file)) {
                    nova::Log("Packing resource [{}], size = {}", virtual_path, nova::ByteSizeToString(size));
                }
            } else {
                auto offset = data.size();
                auto aligned_offset = AlignUpPower2(offset, 256);

                // nova::Log("Entry(path = {}, offset = {}, size = {})", virtual_path, aligned_offset, nova::ByteSizeToString(size));

                entries.emplace_back(Entry {
                    .virtual_path = virtual_path,
                    .offset = aligned_offset,
                    .size = size,
                });

                data.resize(aligned_offset + size);
                std::memcpy(data.data() + aligned_offset, entry_data, size);
            }
        }

// -----------------------------------------------------------------------------

        void PackBinaryFile(StringView path, StringView virtual_path)
        {
            auto contents = files::ReadBinaryFile(path);

            PushEntry(std::string(virtual_path), contents.data(), contents.size());
        }

        void PackTextFile(StringView path, StringView virtual_path)
        {
            auto contents = files::ReadTextFile(path);

            PushEntry(std::string(virtual_path), contents.data(), contents.size() + 1 /* null terminator */);
        }

        void PackSlangFile(StringView path, StringView virtual_path)
        {
            auto contents = files::ReadTextFile(path);

            usz data_size = 0;

            auto text_size = contents.size() + 1; // Null terminator
            PushEntry(std::string(virtual_path), contents.data(), text_size);
            data_size += text_size;

            auto session = compiler.CreateSession(/* use_vfs = */ false, search_dirs);

            auto module = session.Load(path, contents);

            for (auto& entry_point : module.GetEntryPointNames()) {
                auto code = module.GenerateCode(entry_point);
                auto code_size = code.size() * sizeof(u32);
                PushEntry(Fmt("{}:{}", virtual_path, entry_point), code.data(), code_size);
                data_size += code_size;
            }
        }

// -----------------------------------------------------------------------------

        void ScanRoot(const fs::path& root)
        {
            auto rdi = fs::recursive_directory_iterator(root,fs::directory_options::follow_directory_symlink);

            for (auto& entry : rdi) {
                if (!entry.is_regular_file()) continue;

                auto path = entry.path();

                auto ext = path.extension().string();
                std::ranges::transform(ext, ext.data(), [](char c) { return char(std::toupper(c)); });

                if (ext == ".SLANG") {
                    PackSlangFile(path.string(), fs::relative(path, root).generic_string());
                }

                // if (ext == ".CPP" || ext == ".HPP") {
                //     PackTextFile(path.string(), fs::relative(path, root).generic_string());
                // }

                if (ext == ".TTF") {
                    PackBinaryFile(path.string(), fs::relative(path, root).generic_string());
                }

                // if (ext == ".HDR" || ext == ".TGA" || ext == ".PNG" || ext == ".JPG" || ext == ".EXR" || ext == ".GIF") {
                //     PackBinaryFile(path.string(), fs::relative(path, root).generic_string());
                // }
            }
        }
    };

    void Main(std::span<const nova::StringView> args)
    {
        if (args.size() < 2) {
            NOVA_THROW_STACKLESS("Usage: (output) (dirs)...");
        }

        Packer packer;
        packer.Init(args[0]);

        std::ranges::copy(args, std::back_insert_iterator(packer.search_dirs));

        for (auto& arg : std::span(args.begin() + 1, args.end())) {
            packer.ScanRoot(arg);
        }

        packer.Finalize();
    }
}

int main(int argc, char* argv[])
{
    std::vector<nova::StringView> args(&argv[1], &argv[1] + std::max(0, argc - 1));
    Main(args);
}