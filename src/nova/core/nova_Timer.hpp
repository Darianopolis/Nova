#pragma once

#include "nova_Core.hpp"

namespace nova
{
    class Timer
    {
#ifdef NOVA_PLATFORM_WINDOWS
        using NTSTATUS = uint32_t;
        using ZwSetTimerResolutionType = NTSTATUS(__stdcall *)(IN ULONG RequestedResolution, IN BOOLEAN Set, OUT PULONG ActualResolution);

        HANDLE handle;

        struct TimerResolutionGuard
        {
            const ZwSetTimerResolutionType ZwSetTimerResolution;
            std::chrono::nanoseconds minQuantum;

            TimerResolutionGuard()
                : ZwSetTimerResolution{(NTSTATUS(__stdcall*)(ULONG, BOOLEAN, PULONG)) GetProcAddress(GetModuleHandle(L"ntdll.dll"), "ZwSetTimerResolution")}
            {
                NOVA_LOG("Win32 Timer :: Lowering system interrupt period");
                ULONG minPeriod;
                ZwSetTimerResolution(0, true, &minPeriod);

                minQuantum = std::chrono::nanoseconds(minPeriod * 100);
            }

            ~TimerResolutionGuard()
            {
                NOVA_LOG("Win32 Timer :: Resetting system interrupt period");
                ULONG minPeriod;
                ZwSetTimerResolution(0, false, &minPeriod);
            }
        };

    public:
        std::chrono::nanoseconds GetMinQuantum()
        {
            static TimerResolutionGuard timerResolutionGuard;
            return timerResolutionGuard.minQuantum;
        }

        Timer()
        {
            handle = CreateWaitableTimer(nullptr, true, nullptr);
            if (!handle)
                NOVA_THROW("Timer::Timer - Failed to create win32 timer");
        }

        ~Timer()
        {
            if (handle)
                CloseHandle(handle);
        }
#endif

        void Signal()
        {
#ifdef NOVA_PLATFORM_WINDOWS
            LARGE_INTEGER li;
            li.QuadPart = 0;
            SetWaitableTimer(handle, &li, 0, nullptr, nullptr, false);
#endif
        }

        template<class Rep, class Period>
        bool Wait(std::chrono::duration<Rep, Period> duration, bool preempt = false)
        {
            if (duration <= 0ns)
                return false;

            auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);

#ifdef NOVA_PLATFORM_WINDOWS
            if (preempt)
            {
                if (duration < GetMinQuantum())
                    return true;

                nanos -= GetMinQuantum();
            }

            LARGE_INTEGER li;
            li.QuadPart = -(nanos.count() / 100);
            if (!SetWaitableTimer(handle, &li, 0, nullptr, nullptr, false))
                NOVA_THROW("Timer::Wait - Failed to set win32 timer");

            if (auto res = WaitForSingleObject(handle, INFINITE); res != WAIT_OBJECT_0)
                NOVA_THROW("Timer::Wait - Failed to wait on win32 timer");
#else
            std::this_thread::sleep_for(duration);
#endif
            return preempt;
        }
    };
}