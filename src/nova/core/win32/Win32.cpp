#include "Win32.hpp"

#include <shellapi.h>
#include <userenv.h>
#include <shlobj_core.h>
#include <timeapi.h>

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

        for (;;) {
            auto res = ::GetEnvironmentVariableW(wname.c_str(), value.data(), static_cast<DWORD>(value.size() + 1));

            // Failure
            if (res == 0) {
                auto last_error = GetLastError();
                if (last_error == ERROR_ENVVAR_NOT_FOUND) {
                    // No environment variable
                    return {};
                }
                NOVA_THROW(win::HResultToString(HRESULT_FROM_WIN32(last_error)));
            }

            // Environment variable successfully written
            if (res <= value.size()) {
                value.resize(res);
                break;
            }

            // res is required lpBuffer size INCLUDING null terminator character
            value.resize(res - 1);
        }

        return FromUtf16(value);
    }

    fs::path GetExecutablePath()
    {
        wchar_t module_filename[4096];
        if (0 == GetModuleFileNameW(nullptr, module_filename, sizeof(module_filename))) {
            NOVA_THROW(win::LastErrorString());
        }
        return std::filesystem::path(module_filename);
    }

    fs::path GetUserDirectory()
    {
        return fs::path(GetValue("USERPROFILE"));
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

// -----------------------------------------------------------------------------
//                            Win32 Timers
// -----------------------------------------------------------------------------

namespace nova
{
    void SleepFor(std::chrono::nanoseconds nanos)
    {
        // https://blog.bearcats.nl/perfect-sleep-function/

        constexpr int64_t Period    = 8;              // 8ms        in milliseconds
        constexpr int64_t MaxTicks  = Period * 9'500; // 95% of 8ms in ticks (100ns)
        constexpr int64_t Tolerance = 1'020'000;      // 1.2ms      in nanoseconds

        using namespace std::chrono;

        auto t = steady_clock::now();
        auto target = t + nanos;

        NOVA_DO_ONCE() { timeBeginPeriod(Period); };
        thread_local HANDLE timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

        // Sleep

        for (;;) {
            int64_t remaining = (target - t).count();
            int64_t ticks = (remaining - Tolerance) / 100;
            if (ticks <= 0) {
                break;
            }
            if (ticks > MaxTicks) {
                ticks = MaxTicks;
            }

            LARGE_INTEGER due;
            due.QuadPart = -ticks;
            SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, 0);
            WaitForSingleObject(timer, INFINITE);
            t = steady_clock::now();
        }

        // Spin

        while (steady_clock::now() < target) {
            std::this_thread::yield();
        }
    }
}
