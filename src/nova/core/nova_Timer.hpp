#pragma once

#include "nova_Core.hpp"

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
}