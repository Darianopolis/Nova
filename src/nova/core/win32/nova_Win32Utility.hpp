#pragma once

#include "nova_Win32Include.hpp"

#include <nova/core/nova_Debug.hpp>
#include <nova/core/nova_Strings.hpp>

namespace nova::win
{
    template<typename T>
    void Check(T expected, T actual)
    {
        if (expected != actual) {
            NOVA_THROW("Expected {} ({:#x}), got {} ({:#x})", i32(expected), u32(expected), i32(actual), u32(actual));
        }
    }

    template<typename T>
    void CheckNot(T error, T actual)
    {
        if (error == actual) {
            NOVA_THROW("Error case {} ({:#x})", i32(error), u32(error));
        }
    }

    std::string HResultToString(HRESULT res);
}