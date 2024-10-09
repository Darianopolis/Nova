#include <xxhash.h>

#ifdef HARMONY_USE_IMPORT_STD
import std;
import std.compat;
#endif

#include "Build.hpp"
#include "backend/Backend.hpp"

#include <nova/core/Json.hpp>
#include <xxhash.h>

#include <math.h>

static
bool ws(char c)
{
    return c == ' ' || c == '\t';
}

static
bool nl(char c)
{
    return c == '\n' || c == '\r';
}

static
bool wsnl(char c)
{
    return ws(c) || nl(c);
}

ScanResult ScanFile(const fs::path& path, std::string& data, FunctionRef<void(Component&)> callback)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        // TODO: Is this recoverable?
        NOVA_THROW("Failed to open file [{}] for scanning...", path.string());
    }

    auto size = fs::file_size(path);
    in.seekg(0);
    data.resize(size + 16, '\0');
    in.read(data.data(), size);

    std::memset(data.data() + size, '\n', 16);
    data[size + 1] = '"';
    data[size + 2] = '>';
    data[size + 3] = '*';
    data[size + 4] = '/';

    // TODO: Handle strings
    // TODO: Handle basic preprocessor evaluation
    //          #ifndef THING
    //          #if [!]THING
    //          #if [!]defined(THING)
    //          #else
    //          #endif
    //          #define THING
    //          #undef THING
    //          #define THING X

    auto start_time = chr::steady_clock::now();

    auto data_start = data.data();
    auto cur = data_start;
    auto padding_end = cur + data.size();
    auto data_end = cur + size;

    auto EscapeChar = [](const char& c) -> std::string_view
    {
        if (c == '\n') return "\\n";
        if (c == '\t') return "\\t";
        if (c == '\r') return "\\r";
        return std::string_view(&c, 1);
    };

// #define HARONY_BUILD_SCAN_TRACE

#ifdef HARONY_BUILD_SCAN_TRACE
#define HARONY_BUILD_SCAN_LOG_TRACE(...) std::println(__VA_ARGS__)
    auto PeekFalse = [&](const char* c, const std::source_location& loc = std::source_location::current()) -> bool
    {
        HARONY_BUILD_SCAN_LOG_TRACE("<{: 4}>[{: 4}] {}", loc.line(), c - data_start, EscapeChar(*c));
        HARMONY_IGNORE(c)
        return false;
    };
#else
#define HARONY_BUILD_SCAN_LOG_TRACE(...)
    auto PeekFalse = [&](auto&&...) { return false; };
#endif

    auto SkipToEndOfLine = [&] {
        for (;;) {
            while (!nl(*cur)) PeekFalse(cur++);
            // if not escaped break immediately
            auto last = *(cur++ - 1);
            // check and fully escape crlf
            if (*(cur - 1) == '\r' && *cur == '\n') cur++;
            if (last != '\\') break;
            HARONY_BUILD_SCAN_LOG_TRACE("new line escape, continuing...");
        }
    };

    auto SkipWhitespaceAndCommments = [&](const std::source_location& loc = std::source_location::current()) {
        for (;;) {
            PeekFalse(cur, loc);
            if (*cur == '/') {
                if (*(cur + 1) == '/') {
                    HARONY_BUILD_SCAN_LOG_TRACE("Found single line comment, skipping to end of line");
                    SkipToEndOfLine();
                    continue;
                } else if (*(cur + 1) == '*') {
                    HARONY_BUILD_SCAN_LOG_TRACE("Found multi-line comment, skipping to closing comment");
                    cur += 2;
                    for (;;) {
                        PeekFalse(cur);
                        if (*cur == '*') {
                            if (*(cur + 1) == '/') {
                                break;
                            }
                        }
                        cur++;
                    }
                } else {
                    break;
                }
            }

            if (!ws(*cur)) break;

            cur++;
        }
    };

    std::string_view primary_module_name;

    while (cur <= data_end) {
        SkipWhitespaceAndCommments();

        if (*cur == '#') {
            while (ws(*++cur));
            HARONY_BUILD_SCAN_LOG_TRACE("Checking for 'include' directive [{}]", std::string_view(cur, cur + 7));
            if (std::string_view(cur, cur + 7) != "include") {
                HARONY_BUILD_SCAN_LOG_TRACE("  not found, skipping to next unescaped newline");
                SkipToEndOfLine();
                HARONY_BUILD_SCAN_LOG_TRACE("  reached end of preprocessor statement");
                continue;
            }
            HARONY_BUILD_SCAN_LOG_TRACE("Found 'include' directive");
            cur += 6;
            while (ws(*++cur));
            PeekFalse(cur);
            if (*cur != '<' && *cur != '"') continue;
            bool angled = *cur == '<';
            auto start = cur + 1;
            if (angled) {
                while (*++cur != '>');
            } else {
                while (*++cur != '"');
            }
            auto end = cur++;

            LogTrace("#include {}{}{}", angled ? '<' : '"', std::string_view(start, end), angled ? '>' : '"');

            Component comp;
            comp.name = std::string(start, end);
            comp.type = Component::Type::Header;
            comp.imported = true;
            comp.angled = angled;

            callback(comp);
        }

        if (*cur == 'm') {
            HARONY_BUILD_SCAN_LOG_TRACE("Found 'm' checking for module");
            bool is_exported = false;
            bool is_imported = false;
            bool is_header_unit = false;
            bool angled = false;
            const char* module_start = nullptr;
            const char *name_start{}, *name_end{};
            const char *part_start{}, *part_end{};

            if (cur <= data_start || wsnl(*(cur - 1))) {
                if (std::string_view(cur + 1, cur + 6) == "odule") {
                    HARONY_BUILD_SCAN_LOG_TRACE("found 'module' keyword");
                    module_start = cur;
                    cur += 5;
                }
            } else if (*(cur - 1) == 'i' && ((cur - 1) <= data_start || wsnl(*(cur - 2)))) {
                HARONY_BUILD_SCAN_LOG_TRACE("preceeded by 'i', checking for 'import'");
                if (std::string_view(cur - 1, cur + 5) == "import") {
                    HARONY_BUILD_SCAN_LOG_TRACE("found 'import' keyword");
                    is_imported = true;
                    module_start = cur - 1;
                    cur += 4;
                }
            }
            if (!module_start) {
                HARONY_BUILD_SCAN_LOG_TRACE("not module or import, ignoring");
                ++cur; continue;
            }
            PeekFalse(cur);
            ++cur;
            SkipWhitespaceAndCommments();
            PeekFalse(cur);
            if (*cur == ';') {
                // Global module fragment, ignore
                HARONY_BUILD_SCAN_LOG_TRACE("global module fragment, ignore");
                ++cur; continue;
            }
            if (*cur == '"' || *cur == '<') {
                is_header_unit = true;
                angled = *cur == '<';
                name_start = cur + 1;
                if (angled) {
                    while (*++cur != '>');
                } else {
                    while (*++cur != '"');
                }
                name_end = cur;
                ++cur; SkipWhitespaceAndCommments();
                if (*cur != ';') {
                    HARONY_BUILD_SCAN_LOG_TRACE("Expected semicolon to end import statement, found {}", *cur);
                    continue;
                }
            } else if (!wsnl(*(cur - 1))) {
                HARONY_BUILD_SCAN_LOG_TRACE("expected '\"', '<' or whitespace after [module/import] keyword, found {}", *cur);
                continue;
            } else {
                auto IsIdentifierChar = [](char ch) {
                    // return !(wsnl(ch) || ch == ';' || ch == ':');
                    return std::isalnum(ch) || ch == '.' || ch == '_';
                };
                HARONY_BUILD_SCAN_LOG_TRACE("parsing name");
                name_start = cur;
                while (PeekFalse(cur) || IsIdentifierChar(*cur)) cur++;
                name_end = cur;
                HARONY_BUILD_SCAN_LOG_TRACE("end of name: [{}]", std::string_view(name_start, name_end));
                PeekFalse(cur);
                SkipWhitespaceAndCommments();
                PeekFalse(cur);
                if (*cur == ':') {
                    HARONY_BUILD_SCAN_LOG_TRACE("parsing partition name");
                    ++cur; SkipWhitespaceAndCommments();
                    part_start = cur;
                    while (PeekFalse(cur) || IsIdentifierChar(*cur)) cur++;
                    part_end = cur;
                    HARONY_BUILD_SCAN_LOG_TRACE("end of part name: [{}]", std::string_view(part_start, part_end));
                }
                if ((name_start == name_end) && std::string_view(part_start, part_end) == "private") {
                    // start of private module fragment, ignore
                    continue;
                }
                SkipWhitespaceAndCommments();
                PeekFalse(cur);
                if (*cur != ';') {
                    HARONY_BUILD_SCAN_LOG_TRACE("Expected semicolon, not valid module statement");
                    // not a semicolon terminated module statement, ignore
                    continue;
                }
            }
            auto p = module_start - 2;
            while (p >= data_start && !PeekFalse(p) &&wsnl(*p)) p--;
            if (p >= data_start) PeekFalse(cur); else HARONY_BUILD_SCAN_LOG_TRACE("no non-space characters before 'module' keyword");
            if (p - 5 >= data_start) {
                auto str = std::string_view(p - 5, p + 1);
                HARONY_BUILD_SCAN_LOG_TRACE("checking for 'export' keyword in prefix: [{}]", str);
                if (str == "export") {
                    is_exported = true;
                }
            }
            auto name = std::string_view(name_start, name_end);
            auto part = std::string_view(part_start, part_end);
            if (is_imported) {
                if (!part.empty() && name.empty()) {
                    name = primary_module_name;
                }
            } else {
                primary_module_name = name;
            }
            if (is_header_unit) {
                LogTrace("{}import {}{}{};", is_exported ? "export " : "", angled ? '<' : '"', name, angled ? '>' : '"');
            } else {
                LogTrace("{}{} {}{}{};", is_exported ? "export " : "", is_imported ? "import" : "module", name, part.empty() ? "" : "<:>", part);
            }
            if (is_imported && !part.empty() && name != primary_module_name) {
                LogError("Module partition does not belong to primary module: [{}]", primary_module_name);
            }

            Component comp;
            comp.name = part.empty() ? std::string(name) : std::format("{}:{}", name, part);
            comp.type = is_header_unit ? Component::Type::HeaderUnit : Component::Type::Interface;
            comp.exported = is_exported;
            comp.imported = is_imported;
            comp.angled = angled;

            callback(comp);
        }

        ++cur;
    }

    if (cur >= padding_end) NOVA_THROW("Overrun buffer!");

    auto end_time = chr::steady_clock::now();

    LogTrace("Parsed {} in {} ({}/s)", ByteSizeToString(data.size()), DurationToString(end_time - start_time),
        ByteSizeToString(uint64_t(double(data.size()) / chr::duration_cast<chr::duration<double>>(end_time - start_time).count())));

    auto hash = XXH64(data.data(), data.size(), 0);

    return ScanResult {
        .size =  data.size(),
        .hash = hash,
        .unique_name = std::format("{}.{:x}", path.filename().string(), hash),
    };
}

std::optional<fs::path> FindInclude(fs::path path, std::string_view include, bool angled, std::span<const fs::path> include_dirs, BuildState& state, bool& is_system)
{
    is_system = false;

    LogTrace("Searching for header {}{}{} included in [{}]", angled ? '<' : '"', include, angled ? '>' : '"', path.string());
    if (!angled) {
        while (path.has_parent_path()) {
            auto new_path = path.parent_path();
            if (new_path == path) {
                break;
            }
            path = new_path;

            auto target = path / include;

            LogTrace("  \"{}\"", target.string());
            if (fs::exists(target)) {
                LogTrace("    Found!");
                return std::move(target);
            }
        }
    }

    for (auto& include_dir : include_dirs) {
        auto target = include_dir / include;
        LogTrace("  <{}>", target.string());
        if (fs::exists(target)) {
            LogTrace("    Found!");
            return std::move(target);
        }
    }

    for (auto& include_dir : state.system_includes) {
        auto target = include_dir / include;
        LogTrace("  [{}]", target.string());
        if (fs::exists(target)) {
            LogTrace("    Found!");
            is_system = true;
            return std::move(target);
        }
    }

    LogTrace("    Header not found");

    return std::nullopt;
}

void ScanDependencies(BuildState& state, bool use_backend_dependency_scan)
{
    LogInfo("Scanning dependencies");

    std::unordered_map<fs::path, std::string> marked_header_units;

    auto backend_scan_differences = 0;

    std::vector<std::string> dependency_info;
    if (use_backend_dependency_scan) {
        dependency_info.resize(state.tasks.size());
#pragma omp parallel for
        for (uint32_t i = 0; i < state.tasks.size(); ++i) {
            state.backend->FindDependencies(state.tasks[i], dependency_info[i]);
        }
    }

    {
        std::string scan_storage;
        std::unordered_map<std::string, int> produced_set;
        std::unordered_map<std::string, int> required_set;
        for (auto i = 0; i < state.tasks.size(); ++i) {
            auto& task = state.tasks[i];
            LogDebug("Scanning file: [{}]", task.source.path.string());

            if (use_backend_dependency_scan) {
                produced_set.clear();
                required_set.clear();

                LogDebug("  Backend results:");

                JsonDocument doc(dependency_info[i]);
                for (auto rule : doc.root()["rules"]) {
                    for (auto provided : rule["provides"]) {
                        auto logical_name = provided["logical-name"].string();
                        produced_set[logical_name]++;
                        LogTrace("produces module [{}]", logical_name);
                        task.produces.emplace_back(logical_name);
                    }

                    for (auto required : rule["requires"]) {
                        auto logical_name = required["logical-name"].string();
                        required_set[logical_name]++;
                        // LogTrace("  requires: {}", logical_name);
                        task.depends_on.emplace_back(Dependency{.name = std::string(logical_name)});
                        if (auto source_path = required["source-path"]) {
                            auto path = fs::path(source_path.string());
                            // LogTrace("    is header unit - {}", path.string());
                            marked_header_units[path] = logical_name;
                            LogTrace("requires header [{}]", path.string());
                        } else {
                            LogTrace("requires module [{}]", logical_name);
                        }
                    }
                }

                LogDebug("  build-deps results:");
            }

            auto scan_result = ScanFile(task.source.path, scan_storage, [&](Component& comp) {
                if (comp.type == Component::Type::Header) {
                    bool is_system;
                    FindInclude(task.source.path, comp.name, comp.angled, task.inputs->include_dirs, state, is_system);
                } else {
                    // Interface of Header Unit
                    if (!comp.imported && comp.exported) {
                        if (use_backend_dependency_scan) {
                            produced_set[comp.name]--;
                        } else {
                            task.produces.emplace_back(std::move(comp.name));
                        }
                    } else {
                        if (comp.type == Component::Type::HeaderUnit) {
                            bool is_system;
                            auto included = FindInclude(task.source.path, comp.name, comp.angled, task.inputs->include_dirs, state, is_system);
                            if (!included) {
                                NOVA_THROW("Source [{}] imports {}{}{} as header, but no header could be found",
                                    task.unique_name, comp.angled ? '<' : '"', comp.name, comp.angled ? '>' : '"');
                            }
                            auto path = fs::absolute(*included);

                            marked_header_units[path] = comp.name;

                            if (is_system) {
                                // TODO: We should track these per source instead of per target
                                task.target->imported_targets["std"] = DependencyType::Private;
                            }
                        }

                        if (use_backend_dependency_scan) {
                            required_set[comp.name]--;
                        } else {
                            task.depends_on.emplace_back(Dependency{.name = std::move(comp.name)});
                        }
                    }
                }
            });

            task.unique_name = scan_result.unique_name;

            if (use_backend_dependency_scan) {
                for (auto&[r, s] : produced_set) {
                    if (s > 0) {
                        backend_scan_differences++;
                        LogError("MSVC produces [{}] not found by build-scan", r);
                    } else if (s < 0) {
                        backend_scan_differences++;
                        LogError("Found produces [{}] not present in MSVC output", r);
                    }
                }

                for (auto&[p, s] : required_set) {
                    if (s > 0) {
                        backend_scan_differences++;
                        LogError("MSVC requires [{}] not found by build-scan", p);
                    } else if (s < 0) {
                        backend_scan_differences++;
                        LogError("build-scan requires [{}] not present in MSVC output", p);
                    }
                }
            }
        }

        if (backend_scan_differences) {
            NOVA_THROW("Discrepancy between backend and build-scan outputs, aborting compilation");
        }
    }

    LogDebug("Marking header units");

    for (auto& task : state.tasks) {
        auto path = fs::absolute(task.source.path);
        auto iter = marked_header_units.find(path);
        if (iter == marked_header_units.end()) continue;
        task.is_header_unit = true;
        task.produces.emplace_back(iter->second);

        LogTrace("task[{}].is_header_unit = {}", task.source.path.string(), task.is_header_unit);

        marked_header_units.erase(path);
    }

    LogDebug("Generating external header unit tasks");

    {
        for (auto[path, logical_name] : marked_header_units) {
            LogTrace("External header unit[{}] -> {}", path.string(), logical_name);

            auto& task = state.tasks.emplace_back();
            task.source.path = path;
            // TODO: SHOULD ONLY DO THIS FOR SYSTEM HEADERS
            //       If header isn't system header, and isn't tracked - error
            //       We shouldn't depend on this for linking at all
            task.target = &state.targets.at("std");
            task.source.type = SourceType::CppHeader;
            task.is_header_unit = true;
            task.produces.emplace_back(logical_name);
            task.external = true;

            auto path_str = fs::absolute(path.string()).string();
            auto hash = XXH64(path_str.data(), path_str.size(), 0);

            task.unique_name = std::format("{}.{:x}", path.filename().string(), hash);
        }
    }

    LogDebug("Trimming normal header tasks");

    std::erase_if(state.tasks, [](const auto& task) {
        if (!task.is_header_unit && task.source.type == SourceType::CppHeader) {
            LogTrace("Header [{}] is not consumed as a header unit", task.unique_name);
            return true;
        }
        return false;
    });
}
void SortDependencies(BuildState& state)
{
    // TODO: Don't duplicate this, inefficient anyway!
    auto FindTaskForProduced = [&](std::string_view name) -> Task* {
        for (auto& task : state.tasks) {
            for (auto& produced : task.produces) {
                if (produced == name) return &task;
            }
        }
        return nullptr;
    };

    LogDebug("Calculating dependency depths");

    auto FindMaxDepth = [&](this auto&& self, Task& task, uint32_t depth) -> void {
        if (depth <= task.max_depth) {
            // already visited at or deeper
            return;
        }
        task.max_depth = std::max(depth, task.max_depth);

        for (auto& dep : task.depends_on) {
            auto source = FindTaskForProduced(dep.name);
            if (!source) {
                NOVA_THROW(std::format("No task provides [{}] required by [{}]", dep.name, task.unique_name));
            }

            self(*source, depth + 1);
        }
    };

    {
        // uint32_t max_depth  = 0;
        for (auto& task : state.tasks) {
            // max_depth = std::max(max_depth, FindMaxDepth(task));
            FindMaxDepth(task, 1);
        }
    }

    // uint32_t i = 0;
    // auto ReadMaxDepthChain = [&](this auto&& self, Task& task) -> void {
    //     LogTrace("[{}] = {}", i++, task.source.path.string());
    //
    //     uint32_t max_depth = 0;
    //     Task* max_task = nullptr;
    //
    //     for (auto& dep : task.depends_on) {
    //         if (*dep.source->max_depth >= max_depth) {
    //             max_depth = *dep.source->max_depth;
    //             max_task = dep.source;
    //         }
    //     }
    //
    //     if (max_depth > 0) {
    //         self(*max_task);
    //     }
    // };
    //
    // {
    //     uint32_t max_depth = 0;
    //     Task* max_task = nullptr;
    //
    //     for (auto& task : state.tasks) {
    //         if (*task.max_depth >= max_depth) {
    //             max_depth = *task.max_depth;
    //             max_task = &task;
    //         }
    //     }
    //
    //     if (max_task) {
    //         LogDebug("Maximum task depth = {}", max_depth);
    //         ReadMaxDepthChain(*max_task);
    //     }
    // }

    LogDebug("Sorting tasks");

    std::ranges::sort(state.tasks, [](auto& l, auto& r) { return l.max_depth > r.max_depth; });
}
