#include "Platform.h"
#include "Threading/SleepTimer.h"

#include <thread>

#if FLING_WINDOWS
// Windows 10 1803 and later. MinGW headers can be older.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif

namespace Fling
{
    SleepTimer::SleepTimer()
    {
#if FLING_WINDOWS
        m_NativeTimer =
            CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
#endif
    }

    SleepTimer::~SleepTimer()
    {
#if FLING_WINDOWS
        if (m_NativeTimer != nullptr)
        {
            CloseHandle(m_NativeTimer);
        }
#endif
    }

    bool SleepTimer::IsHighResolution() const
    {
#if FLING_WINDOWS
        return m_NativeTimer != nullptr;
#else
        return true;
#endif
    }

    void SleepTimer::SleepUntil(const std::chrono::steady_clock::time_point t_Deadline)
    {
        using Clock = std::chrono::steady_clock;
        // A wake can come before the deadline (a rounded due time), so loop
        for (Clock::time_point Now = Clock::now(); Now < t_Deadline; Now = Clock::now())
        {
#if FLING_WINDOWS
            if (m_NativeTimer != nullptr)
            {
                // A negative due time is relative, in 100 ns units
                const auto Remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(t_Deadline - Now);
                LARGE_INTEGER DueTime;
                DueTime.QuadPart = -static_cast<LONGLONG>((Remaining.count() + 99) / 100);
                if (SetWaitableTimerEx(m_NativeTimer, &DueTime, 0, nullptr, nullptr, nullptr, 0) &&
                    WaitForSingleObject(m_NativeTimer, INFINITE) == WAIT_OBJECT_0)
                {
                    continue;
                }
            }
#endif
            std::this_thread::sleep_until(t_Deadline);
        }
    }
} // namespace Fling
