#pragma once

#include <nova/core/nova_Core.hpp>

// -----------------------------------------------------------------------------
//                          Windows UNICODE Macros
// -----------------------------------------------------------------------------

#ifndef UNICODE
#  define UNICODE
#endif

#ifndef _UNICODE
#  define _UNICODE
#endif

// -----------------------------------------------------------------------------
//                          Windows header includes
// -----------------------------------------------------------------------------

#ifndef NOMINMAX
#  define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

// -----------------------------------------------------------------------------
//                          HRESULT parse helpers
// -----------------------------------------------------------------------------

namespace nova::win
{
    inline
    std::string HResultToString(HRESULT res)
    {
        wchar_t* lp_msg_buf = {};
        NOVA_DEFER(&) { ::LocalFree(lp_msg_buf); };

        DWORD len = ::FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER     // Allocate, only way to handle arbitrary length messages
            | FORMAT_MESSAGE_FROM_SYSTEM       // Generic system error message table
            | FORMAT_MESSAGE_IGNORE_INSERTS    // Can't provide parameters for inserts
            ,
            nullptr,
            res,
            0,
            reinterpret_cast<wchar_t *>(&lp_msg_buf), // Horrible Microsoft API hacks
            0,
            nullptr);

        if (!len) {
            return "Unknown error";
        }

        // Trim trailing whitespace

        while (len > 0) {
            auto c = lp_msg_buf[len - 1];
            if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
                len--;
            } else {
                break;
            }
        }

        auto str = FromUtf16({lp_msg_buf, len});

        return str;
    }

    inline
    std::string LastErrorString()
    {
        return HResultToString(HRESULT_FROM_WIN32(GetLastError()));
    }

    inline
    void Check(HRESULT hres)
    {
        if (FAILED(hres)) {
            NOVA_THROW(LastErrorString());
        }
    }

    inline
    HANDLE Check(HANDLE handle, std::string_view message)
    {
        if (handle == INVALID_HANDLE_VALUE) {
            auto err_str = LastErrorString();
            NOVA_THROW(Fmt("Error {}: {}", message, err_str));
        }
        return handle;
    }
}