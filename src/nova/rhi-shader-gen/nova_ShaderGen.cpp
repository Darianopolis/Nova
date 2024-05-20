#pragma once

#include <nova/core/nova_ToString.hpp>
#include <nova/core/nova_Strings.hpp>
#include <nova/core/nova_Files.hpp>

#include <slang.h>
#include <slang-com-ptr.h>

namespace nova
{
    struct Packer
    {
        Slang::ComPtr<slang::IGlobalSession> slang_global_session;
        slang::TargetDesc                             target_desc;
        slang::SessionDesc                           session_desc;

        std::stringstream output;

        static constexpr const char RawStringSeqChars[63] { "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" };
        static constexpr u32 RawStringSeqLen = 16;
        std::mt19937 rng{0};
        std::uniform_int_distribution<u32> dist{0, sizeof(RawStringSeqChars) - 2};

        std::string seq_end;

        u32 resource_idx = 0;

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

            output << "#include <nova/vfs/nova_VirtualFilesystem.hpp>\n";

            seq_end = GenerateRawEndString();
        }

        void Write()
        {
            auto path = ".vfs-pack-output/vfs_pack_output.cpp";

            auto new_contents = output.str();

            nova::Log("vfs-pack-output:\n  size = {}", nova::ByteSizeToString(new_contents.size()));

            if (fs::exists(path)) {
                auto prev_contents = files::ReadTextFile(path);
                if (prev_contents == new_contents) {
                    nova::Log("No change in pack!");
                    return;
                }
            }

            std::filesystem::create_directories(".vfs-pack-output");

            std::ofstream file(path, std::ios::binary);
            file.write(new_contents.data(), new_contents.size());
        }

        std::string GenerateRawEndString()
        {
            std::string out;
            out.resize(RawStringSeqLen + 2);
            out[0] = ')';
            out[1 + RawStringSeqLen] = '\"';
            for (u32 i = 0; i < RawStringSeqLen; ++i) {
                out[i + 1] = char(RawStringSeqChars[dist(rng)]);
            }
            return out;
        }

        void PackBinaryFile(StringView path, StringView virtual_path)
        {
            auto contents = files::ReadBinaryFile(path);

            nova::Log("Packing binary file [{}], size = {}", path, contents.size());

            u32 index = 0;
            constexpr u32 WrapIndex = 64;

            output << "static std::monostate nova_vfs_resource" << resource_idx++ << " = nova::vfs::detail::RegisterUC8(\"" << virtual_path << "\", std::array<nova::uc8, " << contents.size() << ">{\n";
            for (u32 i = 0; i < contents.size(); ++i) {
                output << u32(uc8(contents[i])) << ",";
                if (index++ >= WrapIndex) {
                    output << '\n';
                    index = 0;
                }
            }
            output << "});\n";
        }

        void PackTextFile(StringView path, StringView virtual_path)
        {
            auto contents = files::ReadTextFile(path);

            nova::Log("Packing text file [{}], size = {}", path, contents.size());

            // std::string seq_end;
            // do {
            //     seq_end = GenerateRawEndString();
            // } while (contents.contains(seq_end));

            std::string_view seq = std::string_view(seq_end).substr(1, RawStringSeqLen);

            output << "static std::monostate nova_vfs_resource" << resource_idx++ << " = nova::vfs::detail::Register(\"" << virtual_path << "\", nova::vfs::FromString(\n";
            output << "R\"" << seq << "(" << contents << seq_end << "));\n";
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

                if (path.extension().string() == ".slang") {
                    PackTextFile(path.string(), fs::relative(path, root).generic_string());
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

        packer.Write();
    }
}

int main(int argc, char* argv[])
{
    std::vector<nova::StringView> args(&argv[1], &argv[1] + std::max(0, argc - 1));
    Main(args);
}