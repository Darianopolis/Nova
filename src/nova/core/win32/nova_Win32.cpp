#include "nova_Win32.hpp"

#include "shellapi.h"

namespace {
    std::monostate Win32_EnableUTF8 = []() -> std::monostate {
        ::SetConsoleOutputCP(CP_UTF8);
        return {};
    }();
}

// -----------------------------------------------------------------------------
//                          Win32 Virtual Allocation
// -----------------------------------------------------------------------------

namespace nova
{
    void* AllocVirtual(AllocationType type, usz size)
    {
        DWORD win_type = {};
        if (type >= AllocationType::Commit)  win_type |= MEM_COMMIT;
        if (type >= AllocationType::Reserve) win_type |= MEM_RESERVE;
        return VirtualAlloc(nullptr, size, win_type, PAGE_READWRITE);
    }

    void FreeVirtual(FreeType type, void* ptr, usz size)
    {
        DWORD win_type = {};
        if (type >= FreeType::Decommit) win_type |= MEM_DECOMMIT;
        if (type >= FreeType::Release)  win_type |= MEM_RELEASE;
        VirtualFree(ptr, size, win_type);
    }
}

// -----------------------------------------------------------------------------
//                            Win32 Environment
// -----------------------------------------------------------------------------

namespace nova::env
{
    std::string GetValue(StringView name)
    {
        std::wstring wname = ToUtf16(name);
        std::wstring value;

        value.resize(value.capacity());

        // Attempt to query environment variable.
        // Success when `res == value.size()` (NOT INCLUDING null terminator character)
        DWORD res;
        while ((res = ::GetEnvironmentVariableW(wname.c_str(), value.data(), DWORD(value.size() + 1))) > value.size()) {

            // No environment variable
            if (res == 0) return {};

            // res is required lpBuffer size INCLUDING null terminator character
            value.resize(res - 1);
        }

        value.resize(res);

        return FromUtf16(value);
    }

    fs::path GetExecutablePath()
    {
        char module_filename[4096];
        GetModuleFileNameA(nullptr, module_filename, sizeof(module_filename));
        return std::filesystem::path(module_filename);
    }

    std::string GetCmdLineArgs()
    {
        return FromUtf16(GetCommandLineW());
    }

    std::vector<std::string> ParseCmdLineArgs(StringView args)
    {
        int num_args = 0;
        auto arg_list = CommandLineToArgvW(ToUtf16(args).c_str(), &num_args);
        if (!arg_list) {
            NOVA_THROW(win::LastErrorString());
        }
        NOVA_DEFER(&) { LocalFree(arg_list); };

        std::vector<std::string> out;
        for (int i = 0; i < num_args; ++i) {
            out.emplace_back(FromUtf16(arg_list[i]));
        }

        return out;
    }
}