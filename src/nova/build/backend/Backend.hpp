#pragma once

#include <nova/core/Core.hpp>
#include "../Build.hpp"

#ifndef HARMONY_USE_IMPORT_STD
#include <span>
#include <random>
#endif

struct Backend {
    virtual ~Backend() = 0;

    virtual void FindDependencies(const Task& task, std::string& dependency_info_p1689_json) const
    {
        NOVA_IGNORE(task)
        NOVA_IGNORE(dependency_info_p1689_json)
        NOVA_THROW("FindDependencies is not implemented");
    }

    virtual void GenerateStdModuleTasks(Task* std_task, Task* std_compat_task) const
    {
        NOVA_IGNORE(std_task)
        NOVA_IGNORE(std_compat_task)
        NOVA_THROW("GenerateStdModuleTasks is not implemented");
    }

    virtual void AddTaskInfo(std::span<Task> tasks) const
    {
        NOVA_IGNORE(tasks)
        NOVA_THROW("AddTaskInfo is not implemented");
    }

    virtual bool CompileTask(const Task& task) const
    {
        NOVA_IGNORE(task)
        NOVA_THROW("CompileTask is not implemented");
    }

    virtual bool LinkStep(Target& target, std::span<const Task> tasks) const
    {
        NOVA_IGNORE(target)
        NOVA_IGNORE(tasks)
        NOVA_THROW("LinkStep is not implemented");
    }

    virtual void GenerateCompileCommands(std::span<const Task> tasks) const
    {
        // TODO: Control where output goes
        NOVA_IGNORE(tasks)
        NOVA_THROW("GenerateCompileCommands is not implemented");
    };

    virtual void AddSystemIncludeDirs(BuildState& state) const
    {
        NOVA_IGNORE(state);
        NOVA_THROW("AddSystemIncludeDirs is not implemented");
    }
};

inline
Backend::~Backend() = default;
