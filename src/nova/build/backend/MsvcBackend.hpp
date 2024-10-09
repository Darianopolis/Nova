#pragma once

#include "Backend.hpp"

struct MsvcBackend : Backend
{
    MsvcBackend();
    ~MsvcBackend() final;

    void FindDependencies(const Task& task, std::string& dependency_info_p1689_json) const final;
    void GenerateStdModuleTasks(Task* std_task, Task* std_compat_task) const final;
    void AddTaskInfo(std::span<Task> tasks) const final;
    bool CompileTask(const Task& task) const final;
    bool LinkStep(Target& target, std::span<const Task> tasks) const final;
    void AddSystemIncludeDirs(BuildState& state) const final;
};
