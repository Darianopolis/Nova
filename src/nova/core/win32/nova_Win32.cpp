#include "nova_Win32Include.hpp"

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Debug.hpp>

namespace {
    std::monostate Win32_EnableUTF8 = []() -> std::monostate {
        ::SetConsoleOutputCP(CP_UTF8);
        return {};
    }();
}

namespace nova
{
    std::string GetEnv(std::string_view name)
    {
        NOVA_STACK_POINT();

        auto name_cstr = NOVA_STACK_TO_CSTR(name);

        auto& stack = nova::detail::GetThreadStack();
        char* begin = reinterpret_cast<char*>(stack.ptr);

        size_t len;
        if (::getenv_s(&len, begin, UINT_MAX, name_cstr)) {
            return std::string();
        } else {
            return std::string(begin, len);
        }
    }
}