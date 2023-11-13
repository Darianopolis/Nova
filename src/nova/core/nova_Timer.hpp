#pragma once

#include "nova_Core.hpp"
#include "nova_Debug.hpp"

namespace nova
{
    class Timer
    {
        void* handle;

        Timer();
        ~Timer();

        void Signal();

        bool WaitNanos(std::chrono::nanoseconds duration, bool preempt = false);

        template<typename Rep, typename Period>
        bool Wait(std::chrono::duration<Rep, Period> duration, bool preempt = false)
        {
            if (duration <= 0ns) {
                return false;
            }

            return WaitNanos(std::chrono::duration_cast<std::chrono::nanoseconds>(duration), preempt);
        }
    };

// -----------------------------------------------------------------------------

    namespace detail
    {
        thread_local inline std::chrono::steady_clock::time_point NovaTimeitLast;
    }

#define NOVA_TIMEIT_RESET() ::nova::detail::NovaTimeitLast = std::chrono::steady_clock::now()

#define NOVA_TIMEIT(...) do {                                                  \
    using namespace std::chrono;                                               \
    NOVA_LOG("- Timeit ({}) :: " __VA_OPT__("[{}] ") "{} - {}",                \
        duration_cast<milliseconds>(steady_clock::now()                        \
            - ::nova::detail::NovaTimeitLast), __VA_OPT__(__VA_ARGS__,) __LINE__, __FILE__); \
    ::nova::detail::NovaTimeitLast = steady_clock::now();                      \
} while (0)
}