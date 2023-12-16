#include "nova_Win32Include.hpp"

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Debug.hpp>

namespace {
    std::monostate Win32_EnableUTF8 = []() -> std::monostate {
        ::SetConsoleOutputCP(CP_UTF8);
        return {};
    }();
}