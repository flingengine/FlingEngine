#pragma once

#include "CoreAPI.h"

#include <chrono>

namespace Fling
{
    /**
     * Sleeps the calling thread until a time on the steady clock, and never returns before
     * it. How late it wakes is the OS timer's resolution: about 0.5 ms with the Windows
     * high-resolution waitable timer (Windows 10 1803 and later), up to one system tick
     * (15.6 ms) on older Windows, and about 0.1 ms on Linux. It does not spin, so it costs
     * no CPU time while it waits. One thread uses it.
     */
    class SleepTimer
    {
    public:
        CORE_API SleepTimer();
        CORE_API ~SleepTimer();

        SleepTimer(const SleepTimer&) = delete;
        SleepTimer& operator=(const SleepTimer&) = delete;

        /** Return at t_Deadline or a little after it. */
        CORE_API void SleepUntil(std::chrono::steady_clock::time_point t_Deadline);

        /** True when the Windows high-resolution timer is in use; always true on other platforms. */
        CORE_API bool IsHighResolution() const;

    private:
#if defined(_WIN32)
        /** The high-resolution waitable timer. Null when the OS does not have it. */
        void* m_NativeTimer = nullptr;
#endif
    };
} // namespace Fling
