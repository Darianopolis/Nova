#pragma once

#include "nova_Core.hpp"
#include "nova_Stack.hpp"

#include <fmt/format.h>
#include <fmt/ostream.h>

namespace nova
{
    struct Exception : std::exception
    {
        Exception(std::string msg,
            const std::source_location& loc = std::source_location::current(),
            std::stacktrace           trace = std::stacktrace::current())
            : message(std::move(msg))
            , source_location(std::move(loc))
            , back_trace(std::move(trace))
        {}

        const char*                     what() const { return message.c_str(); }
        const std::source_location& location() const { return source_location; }
        const std::stacktrace&         stack() const { return back_trace;      }

    private:
        std::string                         message;
        const std::source_location& source_location;
        std::stacktrace                  back_trace;
    };
}

#define NOVA_DEBUG() do {                                                         \
    NOVA_STACK_POINT();                                                           \
    std::cout << NOVA_STACK_FORMAT("    Debug :: {} - {}\n", __LINE__, __FILE__); \
} while (0)

#define NOVA_LOG(fmt, ...) do {                                        \
    NOVA_STACK_POINT();                                                \
    std::cout << NOVA_STACK_FORMAT(fmt"\n" __VA_OPT__(,) __VA_ARGS__); \
} while(0)

#define NOVA_LOGEXPR(expr) do {           \
    std::osyncstream sso(std::cout);      \
    sso << #expr " = " << (expr) << '\n'; \
} while (0)

#define NOVA_FORMAT(fmt_str, ...) \
    fmt::format(fmt_str __VA_OPT__(,) __VA_ARGS__)

#define NOVA_THROW(fmt_str, ...) do {                          \
    auto msg = NOVA_FORMAT(fmt_str __VA_OPT__(,) __VA_ARGS__); \
    std::cout << std::stacktrace::current();                   \
    NOVA_LOG("\nERROR: {}", msg);                              \
    throw ::nova::Exception(std::move(msg));                   \
} while (0)

#define NOVA_ASSERT(condition, fmt_str, ...) do { \
    if (!(condition)) [[unlikely]]                \
        NOVA_THROW(fmt_str, __VA_ARGS__);         \
} while (0)

#define NOVA_ASSERT_NONULL(condition) \
    NOVA_ASSERT(condition, "Expected non-null: " #condition)

// -----------------------------------------------------------------------------

template<class ... DurationT>
struct fmt::formatter<std::chrono::duration<DurationT...>> : fmt::ostream_formatter {};

template<>
struct fmt::formatter<std::stacktrace> : fmt::ostream_formatter {};