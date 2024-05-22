#pragma once

#include <nova/core/nova_ToString.hpp>
#include <nova/core/nova_Strings.hpp>
#include <nova/core/nova_Files.hpp>
#include <nova/core/nova_Base64.hpp>

#include <slang.h>
#include <slang-com-ptr.h>

namespace nova
{
    struct Packer
    {
        Slang::ComPtr<slang::IGlobalSession> slang_global_session;
        slang::TargetDesc                             target_desc;
        slang::SessionDesc                           session_desc;

        static constexpr std::string_view              PackFolder = ".vfs-pack-output";
        ankerl::unordered_dense::set<std::string> generated_files;

        std::string seq_end;

        void Init()
        {
            if (SLANG_FAILED(slang::createGlobalSession(slang_global_session.writeRef()))) {
                NOVA_THROW("slang failed creating global session");
            }

            target_desc = {
                .format = SLANG_SPIRV,
                .profile = slang_global_session->findProfile("glsl460"),
                .flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY,
            };

            session_desc = {
                .targets = &target_desc,
                .targetCount = 1,
                .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
            };

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
        }

        void PackBinaryData(StringView virtual_path, void* data, size_t size_in_bytes)
        {
            auto u64_count = (size_in_bytes + 7) / 8;

            // TODO: This is only meaningful when packing multiple resources into the same file
            u32 resource_idx = 0;

            std::stringstream output;
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

            {
                // TODO: Move this to separate function
                // TODO: ALlow packing of multiple resources into the same file

                auto new_contents = output.str();

                auto hash = nova::hash::Hash(new_contents.data(), new_contents.size());
                auto output_file_name = Fmt("nova_VfsOutput_s{}_h{}.cpp", new_contents.size(), hash);

                generated_files.emplace(output_file_name);

                auto path = fs::path(PackFolder) / output_file_name;

                if (fs::exists(path)) {
                    auto prev_contents = files::ReadTextFile(path.string());
                    if (prev_contents == new_contents) {
                        return;
                    } else {
                        // TODO: Use discriminator to resolve collisions
                        NOVA_THROW("Hash collision!");
                    }
                }

                nova::Log("Packing [{}], size = {}", virtual_path, size_in_bytes);

                std::ofstream file(path, std::ios::binary);
                file.write(new_contents.data(), new_contents.size());
            }
        }

        void PackBinaryFile(StringView path, StringView virtual_path)
        {
            auto contents = files::ReadBinaryFile(path);

            PackBinaryData(virtual_path, contents.data(), contents.size());
        }

        void PackTextFile(StringView path, StringView virtual_path)
        {
            auto contents = files::ReadTextFile(path);

            PackBinaryData(virtual_path, contents.data(), contents.size() + 1 /* null terminator */);
        }

        // void PackSlangFile(const fs::path& path)
        // {
        //     auto path_str = path.string();
        //     auto source = files::ReadTextFile(path.string());

        //     Slang::ComPtr<slang::ISession> session;
        //     if (SLANG_FAILED(slang_global_session->createSession(session_desc, session.writeRef()))) {
        //         NOVA_THROW("slang failed created local session");
        //     }

        //     // Load shader code

        //     slang::IModule* slang_module = nullptr;
        //     {
        //         Slang::ComPtr<slang::IBlob> diagnostics_blob;
        //         slang_module = session->loadModuleFromSourceString("shader", path_str.c_str(), source.c_str(), diagnostics_blob.writeRef());
        //         if (diagnostics_blob) {
        //             Log("{}", (const char*)diagnostics_blob->getBufferPointer());
        //         }
        //         if (!slang_module) {
        //             NOVA_THROW("Expected slang_module, got none");
        //         }
        //     }

        //     // Find all entry points

        //     std::vector<Slang::ComPtr<slang::IEntryPoint>> entry_points;

        //     auto entry_point_count = u32(slang_module->getDefinedEntryPointCount());
        //     for (u32 i = 0; i < entry_point_count; ++i) {
        //         Slang::ComPtr<slang::IEntryPoint> entry_point;
        //         if (SLANG_FAILED(slang_module->getDefinedEntryPoint(i, entry_point.writeRef()))) {
        //             NOVA_THROW("slang failed to read defined entry point");
        //         }

        //         entry_points.push_back(std::move(entry_point));
        //     }

        //     // Create composed program including all entry points

        //     std::vector<slang::IComponentType*> component_types;
        //     component_types.push_back(slang_module);
        //     for (auto& entry_point : entry_points) component_types.push_back(entry_point);

        //     Slang::ComPtr<slang::IComponentType> composed_program;
        //     {
        //         Slang::ComPtr<slang::IBlob> diagnostics_blob;
        //         auto result = session->createCompositeComponentType(
        //             component_types.data(), component_types.size(),
        //             composed_program.writeRef(),
        //             diagnostics_blob.writeRef());

        //         if (diagnostics_blob) {
        //             Log("{}", (const char*)diagnostics_blob->getBufferPointer());
        //         }

        //         if (SLANG_FAILED(result)) {
        //             NOVA_THROW("Slang failed composing program");
        //         }
        //     }

        //     // Reflect on composed program entry points

        //     auto layout = composed_program->getLayout();
        //     {
        //         entry_point_count = u32(layout->getEntryPointCount());
        //         nova::Log("ep_count = {}", entry_point_count);
        //         for (u32 i = 0; i < entry_point_count; ++i) {
        //             auto entry_point = layout->getEntryPointByIndex(i);
        //             nova::Log("Entry point: {}", entry_point->getName());
        //         }
        //     }

        //     // Create SPIR-V for each entry point

        //     for (u32 i = 0; i < entry_point_count; ++i) {
        //         Slang::ComPtr<slang::IBlob> spirv_code;
        //         {
        //             Slang::ComPtr<slang::IBlob> diagnostics_blob;
        //             auto result = composed_program->getEntryPointCode(i, 0, spirv_code.writeRef(), diagnostics_blob.writeRef());

        //             if (diagnostics_blob) {
        //                 Log("{}", (const char*)diagnostics_blob->getBufferPointer());
        //             }

        //             if (SLANG_FAILED(result)) {
        //                 NOVA_THROW("Slang failed compiling shader");
        //             }

        //             nova::Log("Compiled SPIR-V for [{}], size = {}", layout->getEntryPointByIndex(i)->getName(), spirv_code->getBufferSize());
        //         }
        //     }
        // }

        void ScanRoot(const fs::path& root)
        {
            auto rdi = fs::recursive_directory_iterator(root,fs::directory_options::follow_directory_symlink);

            for (auto& entry : rdi) {
                if (!entry.is_regular_file()) continue;

                auto path = entry.path();

                auto ext = path.extension().string();
                std::ranges::transform(ext, ext.data(), [](char c) { return char(std::toupper(c)); });

                if (ext == ".SLANG") {
                    PackTextFile(path.string(), fs::relative(path, root).generic_string());
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