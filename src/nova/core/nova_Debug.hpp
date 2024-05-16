#pragma once

#include "nova_Core.hpp"
#include "nova_Stack.hpp"

#include <fmt/format.h>
#include <fmt/ostream.h>

template<class ... DurationT>
struct fmt::formatter<std::chrono::duration<DurationT...>> : fmt::ostream_formatter {};

template<>
struct fmt::formatter<std::stacktrace> : fmt::ostream_formatter {};

// -----------------------------------------------------------------------------

namespace nova
{
    inline
    void Log(std::string_view str)
    {
        fmt::println("{}", str);
    }

    template<class ...Args>
    void Log(const fmt::format_string<Args...> fmt, Args&&... args)
    {
        fmt::println(fmt, std::forward<Args>(args)...);
    }

    struct Exception : std::exception
    {
        Exception(std::string msg,
            const std::source_location& loc = std::source_location::current(),
            std::stacktrace           trace = std::stacktrace::current())
            : message(std::move(msg))
            , source_location(std::move(loc))
            , back_trace(std::move(trace))
        {
            Log("{}\nError: {}", stack(), what());
        }

        const char*                     what() const { return message.c_str(); }
        const std::source_location& location() const { return source_location; }
        const std::stacktrace&         stack() const { return back_trace;      }

    private:
        std::string                         message;
        const std::source_location& source_location;
        std::stacktrace                  back_trace;
    };
}

#define NOVA_DEBUG() \
    ::nova::Log("    Debug :: {} - {}", __LINE__, __FILE__)

#define NOVA_LOG(fmt_str, ...) \
    ::nova::Log(fmt_str __VA_OPT__(,) __VA_ARGS__)

#define NOVA_FMTEXPR(expr) \
    ::nova::FormatStr(#expr " = {}", (expr))

#define NOVA_THROW(fmt_str, ...) \
    throw ::nova::Exception(::nova::FormatStr(fmt_str __VA_OPT__(,) __VA_ARGS__))

#define NOVA_ASSERT(condition, fmt_str, ...) do { \
    if (!(condition)) [[unlikely]]                \
        NOVA_THROW(fmt_str, __VA_ARGS__);         \
} while (0)

#define NOVA_ASSERT_NONULL(condition) \
    NOVA_ASSERT(condition, "Expected non-null: " #condition)
