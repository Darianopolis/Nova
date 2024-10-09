#include "CMakeGenerator.hpp"

#ifndef HARMONY_USE_IMPORT_STD
#include <ranges>
#endif

// TODO: DELETEME - Only for temporary usage of the link discovery code
#include "../backend/MsvcCommon.hpp"

void GenerateCMake(BuildState& state, const fs::path& workspace_dir)
{
    fs::create_directories(workspace_dir);

    LogInfo("Generating CMake configuration");

    // TODO: CMake doesn't allow files to be referenced that don't share a common path with the CmakeLists.txt they are included from
    //   Solution: Generate symbolic/hard links to trick CMake?
    //   Solution: Generate additional CMakeLists.txt and include them?

    std::ofstream out(workspace_dir / "CMakeLists.txt", std::ios::binary);

    out << "cmake_minimum_required(VERSION 3.30.3)\n";

    out << R"(set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options(
        /Zc:preprocessor
        /Zc:__cplusplus
        /utf-8
        /openmp:llvm
        /EHsc)
project(harmony_generated LANGUAGES CXX C)
)";

    std::vector<std::pair<fs::path, fs::path>> workspace_remappings;

    for (auto&[name, target] : state.targets) {
        auto target_path = fs::absolute(target.dir);
        if (!fs::exists(target_path)) continue;
        auto link_path = workspace_dir / target.name;
        workspace_remappings.emplace_back(target_path, target.name);
    }

    std::unordered_set<Target*> empty_targets;

    for (auto&[_, target] : state.targets) {
        bool empty = false;
        if (target.executable) empty = true;
        if (!empty) {
            for (auto& task : state.tasks) {
                if (task.target != &target) continue;

                empty = true;
            }
        }
        if (!empty) {
            empty_targets.emplace(&target);
        }
    }

    for (auto&[name, target] : state.targets) {

        auto target_path = fs::absolute(target.dir);
        auto link_path = workspace_dir / target.name;
        if (fs::exists(target_path) && !fs::exists(link_path)) {
            LogInfo("Creating symlink from [{}] to [{}]", link_path.string(), target_path.string());
            fs::create_directory_symlink(target_path, link_path);
            LogInfo("  Symlink created");
        }

        auto ReparentToWorkspace = [&workspace_remappings](const fs::path& _in)
        {
            auto in = fs::absolute(_in);
            auto IsSubpath = [](const fs::path& path, const fs::path& base) {
                const auto mismatch_pair = std::mismatch(path.begin(), path.end(), base.begin(), base.end());
                return mismatch_pair.second == base.end();
            };
            for (auto&[path, remapped] : workspace_remappings) {
                LogDebug("Checking if [{}] is subpath of [{}]", in.string(), path.string());
                if (IsSubpath(in, path)) {
                    LogDebug("  Is subpath! relative = [{}]", fs::relative(in, path).string());
                    return remapped / fs::relative(in, path);
                }
            }
            NOVA_THROW("Could not find remapping for [{}]", in.string());
            // return fs::path(target.name) / fs::relative(in, target_path);
        };

        {
            // TODO: Different sources sets in target might have different TU inputs now!
            std::vector<std::string>* defines = nullptr;
            std::vector<fs::path>* includes = nullptr;

            std::vector<Task*> source_tasks;
            std::vector<Task*> module_tasks;

            for (auto& task : state.tasks) {
                if (task.target != &target) continue;

                if (task.source.type == SourceType::CppInterface && !task.produces.empty()) {
                    module_tasks.emplace_back(&task);
                } else {
                    source_tasks.emplace_back(&task);
                }

                defines = &task.inputs->defines;
                includes = &task.inputs->include_dirs;
            }

            // TODO: Reuse empty_targets and avoid storing tasks in prepass!
            bool any_sources = !source_tasks.empty() || !module_tasks.empty();

            if (target.executable) {
                out << "# ------------------------------------------------------------------------------\n";
                out << "add_executable(" << name << ")\n";
            } else if (any_sources) {
                out << "# ------------------------------------------------------------------------------\n";
                out << "add_library(" << name << ")\n";
            }

            if (any_sources) {
                // uint32_t partition = 0;
                // std::unordered_set<fs::path> seen_names;
                //
                // auto PartitionMaybe = [&](const Task& task, std::string_view preamble) {
                //     auto filename = task.source.path.filename();
                //     if (seen_names.contains(filename)) {
                //         out << "        )\n";
                //         out << "add_library(" << name << "-" << (partition + 1) << ")\n";
                //         out << "target_link_libraries(" << name << "-" << (partition + 1) << " PUBLIC " << name << (partition ? std::format("-{}", partition) : "") << ")\n";
                //         out << "target_sources(" << name << "-" << (partition + 1) << '\n';
                //         out << "        " << preamble << '\n';
                //         seen_names.clear();
                //         partition++;
                //     } else {
                //         seen_names.emplace(filename);
                //     }
                // };

                out << "target_sources(" << name << '\n';
                if (!module_tasks.empty()) {
                    out << "        PUBLIC\n";
                    out << "        FILE_SET CXX_MODULES TYPE CXX_MODULES FILES\n";
                    for (auto& task : module_tasks) {
                        // PartitionMaybe(*task, "PUBLIC FILE_SET CXX_MODULES TYPE CXX_MODULES FILES");
                        out << "        " << FormatPath(ReparentToWorkspace(task->source.path)) << " # " << task->max_depth << '\n';
                    }
                }
                if (!source_tasks.empty()) {
                    out << "        PRIVATE\n";
                    for (auto& task : source_tasks) {
                        // PartitionMaybe(*task, "PRIVATE");
                        out << "        " << FormatPath(ReparentToWorkspace(task->source.path)) << "\n";
                    }
                }
                out << "        )\n";
            }

            // Scan for sources with different source types
            // TODO: How to prevent these from override "leaking" out
            for (auto& task : state.tasks) {
                if (task.target != &target) continue;

                // Source type is automatic
                if (task.inputs->type == SourceType::Unknown) continue;

                // Source type matches
                if (task.source.type == task.inputs->type) continue;

                // Headers don't need to be overriden here
                if (task.source.type == SourceType::CppHeader) continue;

                // Interfaces are already explicitly marked with the CXX_MODULE file set
                if (task.source.type == SourceType::CppInterface) continue;

                // See: https://cmake.org/cmake/help/latest/prop_sf/LANGUAGE.html
                out << "set_source_files_properties(" << FormatPath(ReparentToWorkspace(task.source.path)) << " PROPERTIES LANGUAGE CXX)\n";
            }

            if (includes && !includes->empty()) {
                out << "target_include_directories(" << name << '\n';
                out << "        PRIVATE\n";
                for (auto& include : *includes) {
                    out << "        " << FormatPath(ReparentToWorkspace(include)) << '\n';
                }
                out << "        )\n";
            }

            if (defines && !defines->empty()) {
                // TODO: These seem to be leaking back into linked targets??
                out << "target_compile_definitions(" << name << '\n';
                out << "        PRIVATE\n";
                for (auto& define : *defines) {
                    out << "        " << define << '\n';
                }
                out << "        )\n";
            }

            if (!empty_targets.contains(&target) && !target.imported_targets.empty()) {
                out << "target_link_libraries(" << name << '\n';
                out << "        PRIVATE\n";
                for (auto&[import, type] : target.imported_targets) {
                    // TODO: PUBLIC/PRIVATE/INTERFACE
                    if (!empty_targets.contains(&state.targets.at(import))) {
                        out << "        " << import << '\n';
                    }
                }

                msvc::ForEachLink(target, [&](auto& link) {
                    out << "        ${CMAKE_SOURCE_DIR}/" << FormatPath(ReparentToWorkspace(link)) << '\n';
                });

                out << "        )\n";
            }
        }
    }
}
