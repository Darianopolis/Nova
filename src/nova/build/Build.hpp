#pragma once

#include <nova/core/Core.hpp>

using namespace nova;

#ifndef HARMONY_USE_IMPORT_STD
#include <unordered_set>
#include <unordered_map>
#endif

struct Backend;

// -----------------------------------------------------------------------------

// TODO: This should use Platform specific environment helper
inline const fs::path HarmonyDir = "C:/Nova";
inline const fs::path HarmonyDataDir = HarmonyDir / "data";
inline const fs::path HarmonyTempDir = HarmonyDir / "tmp";
inline const fs::path HarmonyObjectDir = HarmonyDir / "out";

// -----------------------------------------------------------------------------

enum class SourceType
{
    Unknown,

    CSource,

    CppSource,
    CppHeader,
    CppInterface,
};

inline
std::string_view SourceTypeToString(SourceType type)
{
    switch (type) {
        case SourceType::Unknown: return "Unknown";
        case SourceType::CppSource: return "C++ Source";
        case SourceType::CppHeader: return "C++ Header";
        case SourceType::CppInterface: return "C++ Interface";
        case SourceType::CSource: return "C Source";
    }
    std::unreachable();
}

struct Source
{
    fs::path path;
    SourceType type = SourceType::Unknown;
};

enum class ExecutableType
{
    Console,
    Window,
};

struct Executable {
    std::string name;
    ExecutableType type = ExecutableType::Console;

    std::optional<fs::path> built_path;
};

struct Git
{
    std::string url;
    std::optional<std::string> branch;
    std::vector<std::string> options;
};

enum class ArchiveType
{
    None,
    Zip,
    TarGz,
};

struct Download
{
    std::string url;
    ArchiveType type = ArchiveType::None;
};

struct CMake
{
    std::vector<std::string> options;
};

enum class DependencyType
{
    Private,
    Public,
    Interface
};

struct TranslationInputs
{
    SourceType type = SourceType::Unknown;
    std::vector<std::string> defines;
    std::vector<fs::path> include_dirs;
    std::vector<fs::path> force_includes;

    void MergeBack(const TranslationInputs& other)
    {
        if (type == SourceType::Unknown) type = other.type;

        // TODO: Deduplicate keys instead of prepending defines
        auto tmp_define = std::move(defines);
        defines = other.defines;
        for (auto& define : tmp_define) defines.emplace_back(define);

        for (auto& include : other.include_dirs) include_dirs.emplace_back(include);
        for (auto& finclude : other.force_includes) force_includes.emplace_back(finclude);
    }
};

struct SourceSet
{
    std::vector<Source> sources;
    TranslationInputs inputs;
};

struct Target {
    std::string name;
    std::unordered_map<std::string, DependencyType> imported_targets;

    std::vector<SourceSet> sources;
    TranslationInputs exported_translation_inputs;

    std::optional<Executable> executable;
    std::vector<fs::path> links;
    std::vector<fs::path> shared;

    // TODO: Data sources should be separated out
    // TODO: Sharing CMake configuration with multiple CMake targets separated over multiple Harmony targets?
    std::optional<Git> git;
    std::optional<Download> download;
    std::optional<CMake> cmake;
    fs::path dir;

    // TODO: This is internal build state, move
    std::unordered_set<Target*> flattened_imports;
};

enum class TaskState
{
    Waiting,
    Compiling,
    Complete,
    Failed,
};

struct Task;

struct Dependency {
    std::string name;
    Task* source;
};

struct Task {
    Target* target;
    Source source;
    TranslationInputs* inputs;

    fs::path bmi;
    fs::path obj;
    std::string unique_name;

    std::vector<std::string> produces;
    std::vector<Dependency> depends_on;
    bool is_header_unit = false;

    TaskState state = TaskState::Waiting;

    uint32_t max_depth = 0;

    bool external = false;
};

struct BuildState
{
    std::vector<Task> tasks;
    std::unordered_map<std::string, Target> targets;
    const Backend* backend;
    std::vector<fs::path> system_includes;
};

void ParseTargetsFile(BuildState& state, const fs::path& file, std::string_view config);
void FetchExternalData(BuildState& state, bool clean, bool update);
void ExpandTargets(BuildState& state);
void ScanDependencies(BuildState& state, bool use_backend_dependency_scan);
void DetectAndInsertStdModules(BuildState& state);
void SortDependencies(BuildState& state);
void Flatten(BuildState& state);
bool Build(BuildState&, bool multithreaded);
void Run(BuildState&, std::string_view to_run);

struct Component
{
    enum class Type {
        Header,
        HeaderUnit,
        Interface,
    };

    std::string name;
    Type type;
    bool exported;
    bool imported;
    bool angled;
};

struct ScanResult
{
    size_t size;
    uint64_t hash;
    std::string unique_name;
};

ScanResult ScanFile(const fs::path& path, std::string& storage, FunctionRef<void(Component&)>);
