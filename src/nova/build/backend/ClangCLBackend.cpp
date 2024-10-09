// TODO: Abstract json writing
#include <yyjson.h>

#ifdef HARMONY_USE_IMPORT_STD
import std;
import std.compat;
#endif

#include "ClangCLBackend.hpp"
#include "MsvcCommon.hpp"

#ifndef HARMONY_USE_IMPORT_STD
#include <cstdlib>
#include <filesystem>
#include <print>
#include <fstream>
#include <unordered_set>
#endif

// TODO: These should be configurable at runtime
constexpr std::string_view ClangScanDepsPath = "D:/Dev/Cloned-Temp/llvm-project/build/bin/clang-scan-deps.exe";
constexpr std::string_view ClangClPath = "D:/Dev/Cloned-Temp/llvm-project/build/bin/clang-cl.exe";

ClangClBackend::ClangClBackend()
{
    msvc::EnsureVisualStudioEnvironment();
}

ClangClBackend::~ClangClBackend() = default;

void ClangClBackend::FindDependencies(const Task& task, std::string& dependency_info_p1689_json) const
{
    auto output_location = HarmonyTempDir / std::format("{}.p1689.json", task.unique_name);

    auto cmd = std::format("{} -format=p1689 -o {} -- {} /std:c++latest /nologo -x c++-module {} ",
        ClangScanDepsPath, msvc::PathToCmdString(output_location), ClangClPath, msvc::PathToCmdString(task.source.path));

    for (auto& include_dir : task.inputs->include_dirs) {
        cmd += std::format(" /I{}", msvc::PathToCmdString(include_dir));
    }

    for (auto& define : task.inputs->defines) {
        cmd += std::format(" /D{}", define);
    }

    LogTrace("{}", cmd);
    std::system(cmd.c_str());
    {
        std::ifstream file(output_location, std::ios::binary);
        auto size = fs::file_size(output_location);
        dependency_info_p1689_json.resize(size, '\0');
        file.read(dependency_info_p1689_json.data(), size);
    }
}

void ClangClBackend::GenerateStdModuleTasks(Task* std_task, Task* std_compat_task) const
{
    msvc::GenerateStdModuleTasks(std_task, std_compat_task);
}

void ClangClBackend::AddTaskInfo(std::span<Task> tasks) const
{
    auto build_dir = HarmonyObjectDir;

    for (auto& task : tasks) {
        auto obj = fs::path(std::format("{}.obj", task.unique_name));
        auto bmi = fs::path(std::format("{}.pcm", task.unique_name));

        task.obj = build_dir / obj;
        task.bmi = build_dir / bmi;
    }
}

bool ClangClBackend::CompileTask(const Task& task) const
{
    auto build_dir = HarmonyObjectDir;

    auto cmd = std::format("cd /d {} && {}", msvc::PathToCmdString(build_dir), ClangClPath);

    std::vector<std::string> cmds;

    cmds.emplace_back(std::format("/c /nologo -Wno-everything /EHsc"));
    switch (task.source.type) {
        break;case SourceType::CSource: cmds.emplace_back(std::format("-x c {}", msvc::PathToCmdString(task.source.path)));
        break;case SourceType::CppSource: cmds.emplace_back(std::format("/std:c++latest -x c++ {}", msvc::PathToCmdString(task.source.path)));
        break;case SourceType::CppHeader: {
            if (task.is_header_unit) {
                cmds.emplace_back(std::format("/std:c++latest -fmodule-header -x c++-header {}", msvc::PathToCmdString(task.source.path)));
            } else NOVA_THROW("Attempted to compile header that isn't being exported as a header unit");
        }
        break;case SourceType::CppInterface: cmds.emplace_back(std::format("/std:c++latest -x c++-module {}", msvc::PathToCmdString(task.source.path)));
        break;default: NOVA_THROW("Cannot compile: unknown source type!");
    }

    cmds.emplace_back("-MD");

    // cmd += " /Zc:preprocessor /utf-8 /DUNICODE /D_UNICODE /permissive- /Zc:__cplusplus";
    // cmds.emplace_back("/Zc:preprocessor /permissive-");
    // cmds.emplace_back("/DWIN32 /D_WINDOWS /EHsc /Ob0 /Od /RTC1 /std:c++latest -MD");

    for (auto& include_dir : task.inputs->include_dirs) {
        cmds.emplace_back(std::format("/I{}", msvc::PathToCmdString(include_dir)));
    }

    for (auto& define : task.inputs->defines) {
        cmds.emplace_back(std::format("/D{}", define));
    }

    // TODO: FIXME - Should this be handled by shared build logic?
    std::unordered_set<std::string_view> seen;
    auto AddDependencies = [&cmds, &seen](this auto&& self, const Task& task) -> void {
        for (auto& depends_on : task.depends_on) {

            if (seen.contains(depends_on.name)) continue;
            seen.emplace(depends_on.name);

            if (depends_on.source->is_header_unit) {
                cmds.emplace_back(std::format("-fmodule-file={}", msvc::PathToCmdString(depends_on.source->source.path), msvc::PathToCmdString(depends_on.source->bmi)));
            } else {
                cmds.emplace_back(std::format("-fmodule-file={}={}", depends_on.name, msvc::PathToCmdString(depends_on.source->bmi)));
            }
            self(*depends_on.source);
        }
    };

    AddDependencies(task);

    if (task.source.type == SourceType::CppInterface || task.is_header_unit) {
        cmds.emplace_back(std::format("-fmodule-output={}", task.bmi.filename().string()));
    }
    if (!task.is_header_unit) {
        cmds.emplace_back(std::format("-o {}", task.obj.filename().string()));
    }

    // Generate cmd string, use command files to avoid cmd line size limit

    msvc::SafeCompleteCmd(cmd, cmds);

    LogTrace("{}", cmd);

    return std::system(cmd.c_str()) == 0;
}

void ClangClBackend::GenerateCompileCommands(std::span<const Task> tasks) const
{
    auto build_dir = HarmonyObjectDir;

    auto doc = yyjson_mut_doc_new(nullptr);

    auto root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);

    for (auto& task : tasks) {
        auto out_task = yyjson_mut_arr_add_obj(doc, root);

        // Deduplicate
        auto cmd = std::format("{}", ClangClPath);

        std::vector<std::string> cmds;

        cmds.emplace_back(std::format("/c /nologo -Wno-everything /EHsc"));
        switch (task.source.type) {
            break;case SourceType::CSource: cmds.emplace_back(std::format("-x c {}", msvc::PathToCmdString(task.source.path)));
            break;case SourceType::CppSource: cmds.emplace_back(std::format("/std:c++latest -x c++ {}", msvc::PathToCmdString(task.source.path)));
            break;case SourceType::CppHeader: {
                if (task.is_header_unit) {
                    cmds.emplace_back(std::format("/std:c++latest -fmodule-header -x c++-header {}", msvc::PathToCmdString(task.source.path)));
                } else NOVA_THROW("Attempted to compile header that isn't being exported as a header unit");
            }
            break;case SourceType::CppInterface: cmds.emplace_back(std::format("/std:c++latest -x c++-module {}", msvc::PathToCmdString(task.source.path)));
            break;default: NOVA_THROW("Cannot compile: unknown source type!");
        }

        // cmd += " /Zc:preprocessor /utf-8 /DUNICODE /D_UNICODE /permissive- /Zc:__cplusplus";
        // cmds.emplace_back("/Zc:preprocessor /permissive-");
        // cmds.emplace_back("/DWIN32 /D_WINDOWS /EHsc /Ob0 /Od /RTC1 /std:c++latest -MD");

        for (auto& include_dir : task.inputs->include_dirs) {
            cmds.emplace_back(std::format("/I{}", msvc::PathToCmdString(include_dir)));
        }

        for (auto& define : task.inputs->defines) {
            cmds.emplace_back(std::format("/D{}", define));
        }

        if (task.source.type == SourceType::CppInterface || task.is_header_unit) {
            cmds.emplace_back(std::format("-fmodule-output={}", task.bmi.filename().string()));
        }
        if (!task.is_header_unit) {
            cmds.emplace_back(std::format("-o {}", task.obj.filename().string()));
        }

        msvc::SafeCompleteCmd(cmd, cmds);

        yyjson_mut_obj_add_strcpy(doc, out_task, "directory", fs::absolute(build_dir).string().c_str());
        yyjson_mut_obj_add_strcpy(doc, out_task, "command", cmd.c_str());
        yyjson_mut_obj_add_strcpy(doc, out_task, "file", fs::absolute(task.source.path).string().c_str());
        yyjson_mut_obj_add_strcpy(doc, out_task, "output", fs::absolute(task.obj).string().c_str());
    }

    {
        yyjson_write_flag flags = YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE;
        yyjson_write_err err;
        yyjson_mut_write_file("compile_commands.json", doc, flags, nullptr, &err);
        if (err.code) {
            std::println("JSON write error ({}): {}", err.code, err.msg);
        }
    }
}

bool ClangClBackend::LinkStep(Target& target, std::span<const Task> tasks) const
{
    return msvc::LinkStep(target, tasks);
}

void ClangClBackend::AddSystemIncludeDirs(BuildState& state) const
{
    msvc::AddSystemIncludeDirs(state);
}
