#include "nova_Win32Include.hpp"

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Debug.hpp>
#include <nova/core/nova_Strings.hpp>
#include <nova/core/nova_Env.hpp>

namespace {
    std::monostate Win32_EnableUTF8 = []() -> std::monostate {
        ::SetConsoleOutputCP(CP_UTF8);
        return {};
    }();
}

namespace nova
{
    std::string GetEnv(StringView name)
    {
        std::wstring wname = ToUtf16(name);
        std::wstring value;

        value.resize(value.capacity());

        // Attempt to query environment variable.
        // Success when `res == value.size()` (NOT INCLUDING null terminator character)
        DWORD res;
        while ((res = GetEnvironmentVariableW(wname.c_str(), value.data(), DWORD(value.size() + 1))) > value.size()) {

            // No environment variable
            if (res == 0) return {};

            // res is required lpBuffer size INCLUDING null terminator character
            value.resize(res - 1);
        }

        value.resize(res);

        return FromUtf16(value);
    }
}