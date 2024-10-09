#define NOMINMAX
#include <Windows.h>

#include "MsvcCommon.hpp"

static const fs::path VisualStudioEnvPath = HarmonyDir / "driver/msvc/env";
static constexpr const char* VCToolsInstallDirEnvName = "VCToolsInstallDir";

namespace msvc
{
    void SetupVisualStudioEnvironment()
    {
        if (!env::GetValue(VCToolsInstallDirEnvName).empty()) {
            LogDebug("Using existing Visual Studio environment");
            return;
        }

        if (!fs::exists(VisualStudioEnvPath)) {
            fs::create_directories(VisualStudioEnvPath.parent_path());
            LogDebug("Generating Visual Studio environment in [{}]", VisualStudioEnvPath.string());
            constexpr std::string_view VcvarsPath = "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat";
            std::system(std::format("\"{}\" && set > {}", VcvarsPath, VisualStudioEnvPath.string()).c_str());
        } else {
            LogDebug("Loading Visual Studio environment from [{}]", VisualStudioEnvPath.string());
        }

        std::ifstream in(VisualStudioEnvPath, std::ios::binary);
        if (!in.is_open()) {
            NOVA_THROW("Could not read environment file!");
        }

        std::string line;
        while (std::getline(in, line)) {
            auto equals = line.find_first_of("=");
            if (equals == std::string::npos) {
                NOVA_THROW(std::format("Invalid comand syntex on line:\n{}", line));
            }
            line[equals] = '\0';
            std::ranges::transform(line, line.data(), [](const char c) {
                // Handle CRLF
                return c == '\r' ? '\0' : c;
            });
            LogTrace("Setting env[{}] = {}", line.c_str(), line.c_str() + equals + 1);
            SetEnvironmentVariableA(line.c_str(), line.c_str() + equals + 1);
        }
    }

    void EnsureVisualStudioEnvironment()
    {
        NOVA_DO_ONCE()
        {
            SetupVisualStudioEnvironment();
        };
    }

    fs::path GetVisualStudioStdModulesDir()
    {
        auto vctoolsdir = env::GetValue(VCToolsInstallDirEnvName);
        if (vctoolsdir.empty()) {
            NOVA_THROW("Not running in a valid VS developer environment");
        }
        return fs::path(vctoolsdir) / "modules";
    }
}

// ---------------------------------------------------------------------------------------------------------------------

namespace msvc
{
    std::string PathToCmdString(const fs::path& path)
    {
        return FormatPath(path, PathFormatOptions::Backward | PathFormatOptions::QuoteSpaces | PathFormatOptions::Absolute);
    }

    void SafeCompleteCmd(std::string& cmd, const std::vector<std::string>& cmds)
    {
        auto cmd_dir = HarmonyTempDir;

        size_t cmd_length = cmd.size();
        for (auto& cmd_part : cmds) {
            cmd_length += 1 + cmd_part.size();
        }

        constexpr uint32_t CmdSizeLimit = 4000;

        if (cmd_length > CmdSizeLimit) {
            static std::atomic_uint32_t cmd_file_id = 0;

            fs::create_directories(cmd_dir);
            auto cmd_path = cmd_dir / std::format("cmd.{}", cmd_file_id++);
            std::ofstream out(cmd_path, std::ios::binary);
            for (uint32_t i = 0; i < cmds.size(); ++i) {
                if (i > 0) out.write("\n", 1);
                out.write(cmds[i].data(), cmds[i].size());
            }
            out.flush();
            out.close();

            cmd += " @";
            cmd += PathToCmdString(cmd_path);

        } else {
            for (auto& cmd_part : cmds) {
                cmd += " ";
                cmd += cmd_part;
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

namespace msvc
{
    void GenerateStdModuleTasks(Task* std_task, Task* std_compat_task)
    {
        EnsureVisualStudioEnvironment();

        auto modules_dir = GetVisualStudioStdModulesDir();

        if (!fs::exists(modules_dir)) {
            NOVA_THROW("VS modules dir not found. Please install the C++ Modules component for Visual Studio");
        }

        LogTrace("Found std modules in [{}]", modules_dir.string());

        if (std_task) {
            std_task->source.path = modules_dir / "std.ixx";
        }

        if (std_compat_task) {
            std_compat_task->source.path = modules_dir / "std.compat.ixx";
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

namespace msvc
{
    bool LinkStep(Target& target, std::span<const Task> tasks)
    {
        auto& executable = target.executable.value();

        auto output_file = HarmonyObjectDir / target.name / std::format("{}.exe", executable.name);
        executable.built_path = output_file;
        fs::create_directories(output_file.parent_path());

        auto cmd = std::format("cd /d {} && link /nologo", msvc::PathToCmdString(output_file.parent_path()));
        std::vector<std::string> cmds;
        switch (executable.type) {
            break;case ExecutableType::Console: cmds.emplace_back("/subsystem:console");
            break;case ExecutableType::Window: cmds.emplace_back("/subsystem:window");
        }
        cmds.emplace_back(std::format("/OUT:{}", output_file.filename().string()));
        for (auto& task : tasks) {
            // LogTrace("maybe adding object for {} ({}) target = {} ({})", task.unique_name, task.obj.string(), (void*)task.target, task.target->name);
            if (!target.flattened_imports.contains(task.target) && &target != task.target) {
                // LogTrace("  not imported, skipping..");

                continue;
            }
            if (!fs::exists(task.obj)) {
                LogWarn("Could not find obj for [{}]", task.unique_name);
            }
            cmds.emplace_back(msvc::PathToCmdString(task.obj));
        }

        cmds.emplace_back("user32.lib");
        cmds.emplace_back("gdi32.lib");
        cmds.emplace_back("shell32.lib");
        cmds.emplace_back("Winmm.lib");
        cmds.emplace_back("Advapi32.lib");
        cmds.emplace_back("Comdlg32.lib");
        cmds.emplace_back("comsuppw.lib");
        cmds.emplace_back("onecore.lib");

        cmds.emplace_back("D3D12.lib");
        cmds.emplace_back("DXGI.lib");
        cmds.emplace_back("dcomp.lib");
        cmds.emplace_back("d3d11.lib");

        msvc::ForEachLink(target, [&](auto& link) {
            cmds.emplace_back(msvc::PathToCmdString(link));
        });

        msvc::SafeCompleteCmd(cmd, cmds);

        LogTrace("{}", cmd);

        return std::system(cmd.c_str()) == 0;
    }

    void AddSystemIncludeDirs(BuildState& state)
    {
        auto includes = env::GetValue("INCLUDE");
        if (includes.empty()) NOVA_THROW("Not in valid VS enviornment, missing 'INCLUDE' environment variable");

        auto start = 0;
        while (start < includes.size()) {
            auto sep = includes.find_first_of(';', start);
            auto end = std::min(sep, includes.size());
            auto include = includes.substr(start, end - start);
            LogTrace("Found system include dir: [{}]", include);
            state.system_includes.emplace_back(std::move(include));
            start = end + 1;
        }
    }

    void ForEachLink(const Target& target, FunctionRef<void(const fs::path&)> callback)
    {
        // TODO: Move this to shared logic!
        // TODO: Backend should only care about identifying .lib extensions
        auto AddLinks = [&](const Target& t) {
            LogTrace("Adding links for: [{}]", t.name);
            for (auto& link : t.links) {
                if (fs::is_regular_file(link)) {
                    LogTrace("    adding: [{}]", link.string());
                    callback(link);
                } else if (fs::is_directory(link)) {
                    LogTrace("  finding links in: [{}]", link.string());
                    for (auto iter : fs::directory_iterator(link)) {
                        auto path = iter.path();
                        if (path.extension() == ".lib") {
                            LogTrace("    adding: [{}]", path.string());
                            callback(path);
                        }
                    }
                } else {
                    LogWarn("link path not found: [{}]", link.string());
                }
            }
        };
        AddLinks(target);
        for (auto* import_target : target.flattened_imports) {
            AddLinks(*import_target);
        }
    }

    void ForEachShared(const Target& target, FunctionRef<void(const fs::path&)> callback)
    {
        // TODO: Move this to shared logic!
        // TODO: Backend should only care about identifying .dll extensions
        auto AddShared = [&](const Target& t) {
            LogTrace("Adding shared libraries for: [{}]", t.name);
            for (auto& shared : t.shared) {
                if (fs::is_regular_file(shared)) {
                    LogTrace("    adding: [{}]", shared.string());
                    callback(shared);
                } else if (fs::is_directory(shared)) {
                    LogTrace("  finding shared libraries in: [{}]", shared.string());
                    for (auto iter : fs::directory_iterator(shared)) {
                        auto path = iter.path();
                        if (path.extension() == ".dll") {
                            LogTrace("    adding: [{}]", path.string());
                            callback(path);
                        }
                    }
                } else {
                    LogWarn("shared library path not found: [{}]", shared.string());
                }
            }
        };
        AddShared(target);
        for (auto* import_target : target.flattened_imports) {
            AddShared(*import_target);
        }
    }
}
