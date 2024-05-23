#pragma once

#include <nova/core/nova_ToString.hpp>
#include <nova/core/nova_Strings.hpp>
#include <nova/core/nova_Files.hpp>
#include <nova/core/nova_Base64.hpp>

#include <nova/rhi/slang/nova_SlangCompiler.hpp>

namespace nova
{
    struct PackFile
    {
        std::stringstream output;
        u32         resource_idx = 0;

        void AppendResource(StringView virtual_path, void* data, size_t size_in_bytes)
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
        SlangCompiler compiler;

        std::vector<StringView> search_dirs;

        static constexpr std::string_view              PackFolder = ".vfs-pack-output";
        ankerl::unordered_dense::set<std::string> generated_files;

        usz total_packed_size = 0;

        std::string seq_end;

        void Init()
        {
            fs::create_directories(PackFolder);
        }

        void RemoveStalePackFiles()
        {
            for (auto& path : fs::directory_iterator(PackFolder)) {
                auto filename = path.path().filename().string();
                if (!generated_files.contains(filename)) {
                    // nova::Log("Removing pack file [{}]", filename);
                    fs::remove(path.path());
                }
            }

            nova::Log("Total packed assets = {}", nova::ByteSizeToString(total_packed_size));
        }

        usz SavePackFile(const PackFile& pack_file)
        {
            auto new_contents = pack_file.output.str();

            auto hash = nova::hash::Hash(new_contents.data(), new_contents.size());
            auto output_file_name = Fmt("nova_VfsOutput_s{}_h{}.cpp", new_contents.size(), hash);

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

// -----------------------------------------------------------------------------

        void PackBinaryFile(StringView path, StringView virtual_path)
        {
            auto contents = files::ReadBinaryFile(path);

            PackFile pack_file;
            pack_file.AppendResource(virtual_path, contents.data(), contents.size());
            if (SavePackFile(pack_file)) {
                nova::Log("Packing binary file [{}], size = {}", virtual_path, nova::ByteSizeToString(contents.size()));
            }

            total_packed_size += contents.size();
        }

        void PackTextFile(StringView path, StringView virtual_path)
        {
            auto contents = files::ReadTextFile(path);

            PackFile pack_file;
            pack_file.AppendResource(virtual_path, contents.data(), contents.size() + 1 /* null terminator */);
            if (SavePackFile(pack_file)) {
                nova::Log("Packing text file [{}], size = {}", virtual_path, nova::ByteSizeToString(contents.size()));
            }

            total_packed_size += contents.size() + 1;
        }

        void PackSlangFile(StringView path, StringView virtual_path)
        {
            PackFile pack_file;

            auto contents = files::ReadTextFile(path);

            usz data_size = 0;

            auto text_size = contents.size() + 1; // Null terminator
            pack_file.AppendResource(virtual_path, contents.data(), text_size);
            data_size += text_size;

            auto session = compiler.CreateSession(/* use_vfs = */ false, search_dirs);

            auto module = session.Load(path, contents);

            for (auto& entry_point : module.GetEntryPointNames()) {
                auto code = module.GenerateCode(entry_point);
                auto code_size = code.size() * sizeof(u32);
                pack_file.AppendResource(Fmt("{}:{}", virtual_path, entry_point), code.data(), code_size);
                data_size += code_size;
            }

            if (SavePackFile(pack_file)) {
                nova::Log("Packing slang file [{}], size = {}", virtual_path, nova::ByteSizeToString(data_size));
            }

            total_packed_size += data_size;
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
            }
        }
    };

    void Main(std::span<const nova::StringView> args)
    {
        if (args.size() < 1) {
            NOVA_THROW_STACKLESS("Usage: Root search dirs...");
        }

        Packer packer;
        packer.Init();

        std::ranges::copy(args, std::back_insert_iterator(packer.search_dirs));

        for (auto& arg : args) {
            packer.ScanRoot(arg);
        }

        packer.RemoveStalePackFiles();
    }
}

int main(int argc, char* argv[])
{
    std::vector<nova::StringView> args(&argv[1], &argv[1] + std::max(0, argc - 1));
    Main(args);
}