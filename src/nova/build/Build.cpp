#ifdef HARMONY_USE_IMPORT_STD
import std;
import std.compat;
#endif

#include "build.hpp"

#ifndef HARMONY_USE_IMPORT_STD
#include <print>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <fstream>
#endif

#include "backend/backend.hpp"

// TODO: Move to generic logic
#include "backend/MsvcCommon.hpp"

void DetectAndInsertStdModules(BuildState& state)
{
    LogInfo("Checking for standard modules");

    // TODO: Create std as a fully fledged target without a bunch of special case logic
    auto& std_target = state.targets.at("std");
    std_target.name = "std";

    auto& source_set = std_target.sources.emplace_back();
    source_set.inputs.type = SourceType::CppSource;

    // TODO: Cleanup
    {
        std::optional<Task> std_task, std_compat_task;
        for (auto& task : state.tasks) {
            for (auto& depends_on : task.depends_on) {
                if (depends_on.name == "std") {
                    if (!std_task) {
                        LogDebug("Detected usage of [std] module");
                        std_task = Task {
                            .target = &std_target,
                            .source{.type = SourceType::CppInterface},
                            .inputs = &source_set.inputs,
                            .unique_name = "std",
                            .produces = { "std" },
                            .external = true,
                        };
                    }

                    task.target->imported_targets["std"] = DependencyType::Private;
                }

                if (depends_on.name == "std.compat") {
                    if (!std_compat_task) {
                        LogDebug("Detected usage of [std.compat] module");
                        std_compat_task = Task {
                            .target = &std_target,
                            .source{.type = SourceType::CppInterface},
                            .inputs = &source_set.inputs,
                            .unique_name = "std.compat",
                            .produces = { "std.compat" },
                            .depends_on = { {"std"} },
                            .external = true,
                        };
                    }

                    task.target->imported_targets["std"] = DependencyType::Private;
                }
            }
        }

        state.backend->GenerateStdModuleTasks(
            std_task ? &*std_task : nullptr,
            std_compat_task ? &*std_compat_task : nullptr);

        if (std_task || std_compat_task) {
            std_target.dir = std_task->source.path.parent_path();

            source_set.sources.emplace_back(std_task->source);
            state.tasks.emplace_back(std::move(*std_task));

            if (std_compat_task) {
                if (std_target.dir != std_compat_task->source.path.parent_path()) {
                    NOVA_THROW("std and std compat modules must be in the same folder!");
                }

                source_set.sources.emplace_back(std_compat_task->source);
                state.tasks.emplace_back(std::move(*std_compat_task));
            }
        }
    }
}

void Flatten(BuildState& state)
{
    for (auto&[name, target] : state.targets) {
        LogDebug("Flattening imports for {}", name);

        std::unordered_set<Target*> flattened;

        [&](this auto&& self, Target& cur) -> void {
            if (flattened.contains(&cur)) return;
            if (&cur != &target) {
                // Emplace if not self
                flattened.emplace(&cur);
            }
            LogTrace(" - {}", cur.name);

            for (auto&[import_name, type] : cur.imported_targets) {
                // TODO: link vs source imports need to be handled separately
                try {
                    auto& imported = state.targets.at(import_name);

                    self(imported);
                } catch (std::exception& e) {
                    NOVA_THROW(e.what());
                }
            }
        }(target);

        target.flattened_imports = std::move(flattened);
    }
}

bool Build(BuildState& state, bool multithreaded)
{
    LogInfo("Building");

    LogDebug("Resolving dependencies");

    {
        // TODO: We should handle this per target, unbuilt targets may remain unexpanded and only contain
        //       output information
        auto FindTaskForProduced = [&](std::string_view name) -> Task* {
            for (auto& task : state.tasks) {
                for (auto& produced : task.produces) {
                    if (produced == name) return &task;
                }
            }
            return nullptr;
        };

        for (auto& task : state.tasks) {
            for (auto& depends_on : task.depends_on) {
                auto* depends_on_task = FindTaskForProduced(depends_on.name);
                // All dependencies guaranteed to be satisfied from build scan dependencies phase
                depends_on.source = depends_on_task;
            }
        }
    }

    // TODO: Check for illegal cycles (both in modules and includes)

    LogDebug("Filling in backend task info");

    state.backend->AddTaskInfo(state.tasks);

    LogDebug("Filtering up-to-date tasks");

    // Filter on immediate file changes

    for (auto& task : state.tasks) {
        // TODO: Filter for *all* tasks unless -clean specified
        // if (!task.external) continue;
        // if (task.target->name == "panta-rhei" || task.target->name == "propolis") continue;

        if (task.is_header_unit) {
            if (!fs::exists(task.bmi) || fs::last_write_time(task.source.path) > fs::last_write_time(task.bmi)) {
                continue;
            }
        } else {
            if (!fs::exists(task.obj) || fs::last_write_time(task.source.path) > fs::last_write_time(task.obj)) {
                continue;
            }
        }

        task.state = TaskState::Complete;
    }

    // Filter on dependent module changes

    {
        std::unordered_map<void*, bool> cache;
        auto CheckForUpdateRequired = [&](this auto&& self, Task& task) {
            if (cache.contains(&task)) {
                return cache.at(&task);
            }
            if (task.state != TaskState::Complete) {
                cache[&task] = true;
                return true;
            }

            for (auto& dep : task.depends_on) {
                if (self(*dep.source)) {
                    task.state = TaskState::Waiting;
                    cache[&task] = true;
                    return true;
                }
            }

            cache[&task] = false;
            return false;
        };

        for (auto& task : state.tasks) {
            CheckForUpdateRequired(task);
        }
    }

    // TODO: Filter on included header changes

    LogDebug("Executing build steps");

    struct CompileStats {
        uint32_t to_compile = 0;
        uint32_t skipped = 0;
        uint32_t compiled = 0;
        uint32_t failed = 0;
    } stats;

    // Record num of skipped tasks for build stats
    for (auto& task : state.tasks) {
        if (task.state == TaskState::Complete) stats.skipped++;
        else if (task.state == TaskState::Waiting) stats.to_compile++;
    }

    // if (true) {
    //     backend.GenerateCompileCommands(tasks);
    //     return;
    // }

    LogInfo("Compiling {} files ({} up to date)", stats.to_compile, stats.skipped);

    auto start = chr::steady_clock::now();

    {
        std::mutex m;
        std::atomic_uint32_t num_started = 0;
        std::atomic_uint32_t num_complete = 0;
        uint32_t last_num_complete = 0;
        uint32_t max_threads = std::max(2u, std::thread::hardware_concurrency()) - 1;

        // bool abort = false;

        for (;;) {
            uint32_t remaining = 0;
            uint32_t launched = 0;

            auto _num_started = num_started.load();
            auto _num_complete = num_complete.load();
            if (_num_started - _num_complete > max_threads) {
                num_complete.wait(_num_complete);
                continue;
            }
            if (last_num_complete == _num_complete && _num_started > _num_complete) {
                // wait if no new complete and at least one task in-flight
                num_complete.wait(_num_complete);
            }
            last_num_complete = _num_complete;

            for (auto& task : state.tasks) {
                auto cur_state = std::atomic_ref(task.state).load();
                if (cur_state == TaskState::Complete || cur_state == TaskState::Failed) continue;

                remaining++;

                if (cur_state != TaskState::Waiting) continue;

                if (!std::ranges::all_of(task.depends_on,
                        [&](auto& dependency) {
                            return std::atomic_ref(dependency.source->state) == TaskState::Complete;
                        })) {
                    continue;
                }

                num_started++;
                launched++;

                task.state = TaskState::Compiling;
                auto DoCompile = [&state, &task, &num_complete] {
                    auto success = state.backend->CompileTask(task);

                    std::atomic_ref(task.state) = success ? TaskState::Complete : TaskState::Failed;

                    num_complete++;
                    num_complete.notify_all();

                    return success;
                };

                if (multithreaded) {
                    std::thread t{DoCompile};
                    t.detach();
                } else {
                    [[maybe_unused]] auto success = DoCompile();
                    // if (success) break;
                    // if (!success) {
                    //     log_error("Ending compilation early after error");
                    //     abort = true;
                    //     break;
                    // }
                }
            }

            // if (abort) {
            //     break;
            // }

            if (remaining
                    && _num_started == _num_complete
                    && launched == 0) {
                uint32_t num_errors = 0;
                for (auto& task : state.tasks) {
                    if (task.state == TaskState::Failed) num_errors++;
                }
                if (num_errors) {
                    LogError("Blocked after {} failed compilations", num_errors);
                    break;
                }

                LogError("Unable to start any additional tasks");
                for (auto& task : state.tasks) {
                    if (task.state == TaskState::Complete) continue;
                    LogError("task[{}] blocked", task.source.path.string());
                    for (auto& dep : task.depends_on) {
                        if (dep.source->state == TaskState::Complete) continue;
                        LogError(" - {}{}", dep.name, dep.source->state == TaskState::Failed ? " (failed)" : "");
                    }
                }
                break;
            }

            if (remaining == 0) {
                break;
            }
        }
    }

    auto end = chr::steady_clock::now();

    LogInfo("Reporting Build Stats");

    {
        uint32_t num_complete = 0;
        for (auto& task : state.tasks) {
            if (task.state == TaskState::Complete) num_complete++;
            else if (task.state == TaskState::Failed) stats.failed++;
        }
        stats.compiled = num_complete - stats.skipped;

        if (stats.skipped) {
            LogInfo("Compiled = {} / {} ({} skipped)", stats.compiled, stats.to_compile, stats.skipped);
        } else {
            LogInfo("Compiled = {} / {}", stats.compiled, stats.to_compile);
        }
        if (stats.failed)  LogWarn("  Failed  = {}", stats.failed);
        if (stats.compiled < stats.to_compile) LogWarn("  Blocked = {}", stats.to_compile - (stats.compiled + stats.failed));
        LogInfo("Elapsed  = {}", DurationToString(end - start));

    }

    if (stats.compiled != stats.to_compile) return false;

    LogInfo("Creating target executables");

    int link_errors = 0;

    for (auto&[_, target] : state.targets) {
        if (!target.executable) continue;

        LogInfo("Linking [{}] from [{}]", target.executable->name, target.name);
        auto res = state.backend->LinkStep(target, state.tasks);
        if (!res) {
            LogError("Error linking [{}] from [{}]", target.executable->name, target.name);
            link_errors++;
            continue;
        }

        // Copy shared artifacts

        bool logged_copy = false;

        auto out_dir = HarmonyObjectDir / target.name;
        msvc::ForEachShared(target, [&](const fs::path& shared) {
            auto to = out_dir / shared.filename();
            bool to_exists = fs::exists(to);
            bool do_copy = !to_exists || (fs::last_write_time(to) < fs::last_write_time(shared));
            if (!do_copy) return;
            if (!logged_copy) {
                logged_copy = true;
                LogInfo("Copying shared artifacts...");
            }
            if (to_exists) {
                LogTrace("Updating shared artifact: {}", shared.string());
                fs::remove(to);
            } else {
                LogTrace("Copying shared artifact: {}", shared.string());
            }
            fs::copy(shared, to);
        });
    }

    return link_errors == 0;
}

void Run(BuildState& state, std::string_view to_run)
{
    LogInfo("Running target [{}]", to_run);

    auto iter = state.targets.find(std::string(to_run));
    if (iter == state.targets.end()) {
        NOVA_THROW("Target [{}] not found", to_run);
    }
    auto& target = iter->second;
    if (!target.executable || !target.executable->built_path) {
        NOVA_THROW("Target [{}] does not contain built executable", to_run);
    }

    std::string cmd;
    cmd += FormatPath(*target.executable->built_path, PathFormatOptions::Backward | PathFormatOptions::QuoteSpaces | PathFormatOptions::Absolute);
    LogTrace("{}", cmd);
    std::system(cmd.c_str());
}
