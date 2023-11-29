#include "nova_Timer.hpp"

#include <nova/core/win32/nova_MinWinInclude.hpp>

namespace nova
{
#ifdef _WIN32
    using NTSTATUS = uint32_t;
    using ZwSetTimerResolutionType = NTSTATUS(__stdcall *)(IN ULONG RequestedResolution, IN BOOLEAN Set, OUT PULONG ActualResolution);

    struct TimerResolutionGuard
    {
        // https://stackoverflow.com/questions/85122/how-to-make-thread-sleep-less-than-a-millisecond-on-windows/31411628#31411628
        const ZwSetTimerResolutionType ZwSetTimerResolution;
        std::chrono::nanoseconds                min_quantum;

        TimerResolutionGuard()
            : ZwSetTimerResolution{ZwSetTimerResolutionType(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "ZwSetTimerResolution"))}
        {
            ULONG min_period;
            ZwSetTimerResolution(0, true, &min_period);

            min_quantum = std::chrono::nanoseconds(min_period * 100);
        }

        ~TimerResolutionGuard()
        {
            ULONG min_period;
            ZwSetTimerResolution(0, false, &min_period);
        }
    };

    std::chrono::nanoseconds GetMinQuantum()
    {
        static TimerResolutionGuard timer_resolution;
        return timer_resolution.min_quantum;
    }

    Timer::Timer()
    {
        handle = CreateWaitableTimerW(nullptr, true, nullptr);
        if (!handle) {
            NOVA_THROW("Failed to create Win32 timer");
        }
    }

    Timer::~Timer()
    {
        if (handle) {
            CloseHandle(handle);
        }
    }
#endif

    void Timer::Signal()
    {
#ifdef _WIN32
        LARGE_INTEGER li;
        li.QuadPart = 0;
        SetWaitableTimer(handle, &li, 0, nullptr, nullptr, false);
#endif
    }

    bool Timer::WaitNanos(std::chrono::nanoseconds duration, bool preempt)
    {
        if (duration <= 0ns) {
            return false;
        }

        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);

#ifdef _WIN32
        if (preempt) {
            if (duration < GetMinQuantum()) {
                return true;
            }

            nanos -= GetMinQuantum();
        }

        LARGE_INTEGER li;
        li.QuadPart = -(nanos.count() / 100);
        if (!SetWaitableTimer(handle, &li, 0, nullptr, nullptr, false)) {
            NOVA_THROW("Timer::Wait - Failed to set Win32 timer");
        }

        if (auto res = WaitForSingleObject(handle, INFINITE); res != WAIT_OBJECT_0) {
            NOVA_THROW("Timer::Wait - Failed to wait on Win32 timer");
        }
#else
        std::this_thread::sleep_for(duration);
#endif
        return preempt;
    }
};