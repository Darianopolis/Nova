#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Containers.hpp>
#include <nova/core/nova_Strings.hpp>

#include <slang.h>
#include <slang-com-ptr.h>

namespace nova
{
    struct SlangVirtualFilesystem;
    struct SlangSession;

    struct SlangModule
    {
        SlangSession*        session;
        slang::IModule* slang_module;

        std::vector<std::string> GetEntryPointNames();

        std::vector<u32> GenerateCode(StringView entry_point);
    };

    struct SlangSession
    {
        std::unique_ptr<SlangVirtualFilesystem> vfs;
        Slang::ComPtr<slang::ISession>      session;

        ~SlangSession();

        std::vector<std::string> GetLoadedPaths();

        SlangModule Load(StringView module_name, StringView src = {});
    };

    struct SlangCompiler
    {
        Slang::ComPtr<slang::IGlobalSession> slang_global_session;

        SlangCompiler();

        SlangSession CreateSession(bool use_vfs = true, Span<StringView> search_dirs = {});
    };
}