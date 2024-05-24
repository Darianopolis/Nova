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

        ::FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER   // Allocate, only way to handle arbitrary length messages
            | FORMAT_MESSAGE_FROM_SYSTEM     // Generic system error message table
            | FORMAT_MESSAGE_IGNORE_INSERTS  // Can't provide parameters for inserts
            | FORMAT_MESSAGE_MAX_WIDTH_MASK, // Don't include additional line breaks in message
            nullptr,
            res,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
            reinterpret_cast<wchar_t *>(&lp_msg_buf),  // Horrible Microsoft API hacks
            0,
            nullptr);

        NOVA_DEFER(&) { ::LocalFree(lp_msg_buf); };

        auto str = FromUtf16(lp_msg_buf);

        return str;
    }

    inline
    std::string LastErrorString()
    {
        return HResultToString(HRESULT_FROM_WIN32(GetLastError()));
    }
}