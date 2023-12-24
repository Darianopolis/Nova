#pragma once

#include "nova_Win32Include.hpp"

#include <nova/core/nova_Debug.hpp>

namespace nova::win
{
    template<class T>
    void Check(T expected, T actual)
    {
        if (expected != actual) {
            NOVA_THROW("Expected {} ({:#x}), got {} ({:#x})", i32(expected), u32(expected), i32(actual), u32(actual));
        }
    }

    template<class T>
    void CheckNot(T error, T actual)
    {
        if (error == actual) {
            NOVA_THROW("Error case {} ({:#x})", i32(error), u32(error));
        }
    }

    inline
    void DebugRes(HRESULT res)
    {
        NOVA_LOG("Debug res");
        wchar_t* lp_msg_buf;

        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            res,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<wchar_t *>(&lp_msg_buf), // Really MS?
            0, nullptr);

        NOVA_STACK_POINT();

        NOVA_LOG("Message: {}", NOVA_STACK_FROM_UTF16(lp_msg_buf));

        LocalFree(lp_msg_buf);
    }
}