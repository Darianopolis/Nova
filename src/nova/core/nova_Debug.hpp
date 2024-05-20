#pragma once

#include "nova_Core.hpp"
#include "nova_Stack.hpp"

#include <fmt/format.h>
#include <fmt/ostream.h>

template<typename ... DurationT>
struct fmt::formatter<std::chrono::duration<DurationT...>> : fmt::ostream_formatter {};

template<>
struct fmt::formatter<std::stacktrace> : fmt::ostream_formatter {};

// -----------------------------------------------------------------------------

#define NOVA_ENABLE_STACK_TRACE

namespace nova
{
    inline
    void Log(std::string_view str)
    {
        fmt::println("{}", str);
    }

    template<typename ...Args>
    void Log(const fmt::format_string<Args...> fmt, Args&&... args)
    {
        fmt::println(fmt, std::forward<Args>(args)...);
    }

    struct Exception : std::exception
    {
        struct NoStack {};

        Exception(NoStack,
            std::string msg,
            std::source_location loc = std::source_location::current())
            : message(std::move(msg))
            , source_location(std::move(loc))
        {
            LogContents();
        }

        Exception(std::string msg,
            std::source_location loc = std::source_location::current(),
            std::stacktrace  trace = std::stacktrace::current())
            : message(std::move(msg))
            , source_location(std::move(loc))
            , back_trace(std::move(trace))
        {
            LogContents();
        }

        void LogContents()
        {
            if (HasStack()) {
                Log("────────────────────────────────────────────────────────────────────────────────\n{}", Stack());
            }
            Log("────────────────────────────────────────────────────────────────────────────────\n"
                "Error: {}\n"
                "────────────────────────────────────────────────────────────────────────────────",
                What());
        }

        const char*                     What() const noexcept { return message.c_str(); }
        const std::source_location& Location() const noexcept { return source_location; }
        bool                        HasStack() const noexcept { return back_trace.has_value(); }
        const std::stacktrace&         Stack() const { return back_trace.value(); }

        const char*                     what() const noexcept { return What(); }

    private:
        std::string                         message;
        const std::source_location& source_location;
        std::optional<std::stacktrace>   back_trace;
    };
}

#define NOVA_DEBUG() \
    ::nova::Log("    Debug :: {} - {}", __LINE__, __FILE__)

#define NOVA_FMTEXPR(expr) \
    ::nova::Fmt(#expr " = {}", (expr))

#define NOVA_THROW(fmt_str, ...) \
    throw ::nova::Exception(::nova::Fmt(fmt_str __VA_OPT__(,) __VA_ARGS__))

#define NOVA_THROW_STACKLESS(fmt_str, ...) \
    throw ::nova::Exception(::nova::Exception::NoStack{}, ::nova::Fmt(fmt_str __VA_OPT__(,) __VA_ARGS__));

#define NOVA_ASSERT(condition, fmt_str, ...) do { \
    if (!(condition)) [[unlikely]]                \
        NOVA_THROW(fmt_str, __VA_ARGS__);         \
} while (0)

#define NOVA_UNREACHABLE() \
    NOVA_ASSERT(false, "Unreachable")

#define NOVA_ASSERT_NONULL(condition) \
    NOVA_ASSERT(condition, "Expected non-null: " #condition)
