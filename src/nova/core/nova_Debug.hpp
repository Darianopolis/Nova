#pragma once

#include "nova_Core.hpp"

#define NOVA_DEBUG() \
    std::cout << std::format("    Debug :: {} - {}\n", __LINE__, __FILE__)

#define NOVA_LOG(fmt, ...) \
    std::cout << std::format(fmt"\n" __VA_OPT__(,) __VA_ARGS__)

#define NOVA_LOGEXPR(expr) do {           \
    std::osyncstream sso(std::cout);      \
    sso << #expr " = " << (expr) << '\n'; \
} while (0)

#define NOVA_THROW(fmt, ...) do {                          \
    auto msg = std::format(fmt __VA_OPT__(,) __VA_ARGS__); \
    std::cout << std::stacktrace::current();               \
    NOVA_LOG("\nERROR: {}", msg);                          \
    throw std::runtime_error(std::move(msg));              \
} while (0)

#define NOVA_ASSERT(condition, fmt, ...) do { \
    if (!(condition)) [[unlikely]]            \
        NOVA_THROW(fmt, __VA_ARGS__);         \
} while (0)

#define NOVA_ASSERT_NONULL(condition) \
    NOVA_ASSERT(condition, "Expected non-null: " #condition)