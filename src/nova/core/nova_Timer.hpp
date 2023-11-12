#pragma once

#include "nova_Core.hpp"

namespace nova
{
    class Timer
    {
#ifdef _WIN32
        using NTSTATUS = uint32_t;
        using ZwSetTimerResolutionType = NTSTATUS(__stdcall *)(IN ULONG RequestedResolution, IN BOOLEAN Set, OUT PULONG ActualResolution);

        HANDLE handle;

        struct TimerResolutionGuard
        {
            // https://stackoverflow.com/questions/85122/how-to-make-thread-sleep-less-than-a-millisecond-on-windows/31411628#31411628
            const ZwSetTimerResolutionType ZwSetTimerResolution;
            std::chrono::nanoseconds                min_quantum;

            TimerResolutionGuard()
                : ZwSetTimerResolution{(NTSTATUS(__stdcall*)(ULONG, BOOLEAN, PULONG)) GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "ZwSetTimerResolution")}
            {
                NOVA_LOG("Win32 Timer :: Lowering system interrupt period");
                ULONG min_period;
                ZwSetTimerResolution(0, true, &min_period);

                min_quantum = std::chrono::nanoseconds(min_period * 100);
            }

            ~TimerResolutionGuard()
            {
                NOVA_LOG("Win32 Timer :: Resetting system interrupt period");
                ULONG min_period;
                ZwSetTimerResolution(0, false, &min_period);
            }
        };

    public:
        std::chrono::nanoseconds GetMinQuantum()
        {
            static TimerResolutionGuard timer_resolution_guard;
            return timer_resolution_guard.min_quantum;
        }

        Timer()
        {
            handle = CreateWaitableTimerW(nullptr, true, nullptr);
            if (!handle) {
                NOVA_THROW("Timer::Timer - Failed to create win32 timer");
            }
        }

        ~Timer()
        {
            if (handle) {
                CloseHandle(handle);
            }
        }
#endif

        void Signal()
        {
#ifdef _WIN32
            LARGE_INTEGER li;
            li.QuadPart = 0;
            SetWaitableTimer(handle, &li, 0, nullptr, nullptr, false);
#endif
        }

        template<typename Rep, typename Period>
        bool Wait(std::chrono::duration<Rep, Period> duration, bool preempt = false)
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
                NOVA_THROW("Timer::Wait - Failed to set win32 timer");
            }

            if (auto res = WaitForSingleObject(handle, INFINITE); res != WAIT_OBJECT_0) {
                NOVA_THROW("Timer::Wait - Failed to wait on win32 timer");
            }
#else
            std::this_thread::sleep_for(duration);
#endif
            return preempt;
        }
    };
}