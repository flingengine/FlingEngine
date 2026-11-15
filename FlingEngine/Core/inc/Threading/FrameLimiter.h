#pragma once

#include "CoreAPI.h"
#include "FlingTypes.h"

#include <functional>
#include <memory>

namespace Fling
{
    class SleepTimer;

    /**
     * Caps a loop at a frame rate (the main thread's frames, [Engine] MaxFps). Call
     * WaitForNextFrame once at the end of each frame: it returns when the next frame is due.
     *
     * The frames stay on a fixed schedule: frame N is due at start + N * period, so a wait
     * that wakes a little late, or a frame that takes a little longer than the period, does
     * not slow the average rate (the next wait is shorter). A frame that ends more than half
     * a period after it was due starts the schedule again from now, so frames never run back
     * to back to catch up.
     *
     * The wait sleeps until SpinNs before the deadline, then spins on the clock until the
     * deadline. The engine's limiter sleeps with a SleepTimer on the steady clock and spins at
     * most DefaultSpinNs (1 ms) a frame: a frame starts within a few microseconds of its time
     * when the sleep wakes within that margin (the high-resolution timer), and at most one
     * timer tick late without it. The spin is its only CPU cost: at most 6% of one core at
     * 60 fps. A test gives its own clock and sleep.
     *
     * It reads only its own clock, so it changes nothing on the game thread and no simulation
     * result. One thread uses it.
     */
    class FrameLimiter
    {
    public:
        /** The clock, in nanoseconds. */
        using NowFunction = std::function<int64()>;

        /** Sleep until about t_UntilNs on that clock. It may wake early or late. */
        using SleepFunction = std::function<void(int64 t_UntilNs)>;

        /** The spin of the engine's limiter before each deadline. */
        static constexpr int64 DefaultSpinNs = 1'000'000;

        /** The steady clock, a SleepTimer, and DefaultSpinNs. */
        CORE_API FrameLimiter();

        /** A limiter on another clock (tests: a virtual clock with no wall-time sleep). */
        CORE_API FrameLimiter(NowFunction t_Now, SleepFunction t_Sleep, int64 t_SpinNs);

        CORE_API ~FrameLimiter();

        FrameLimiter(const FrameLimiter&) = delete;
        FrameLimiter& operator=(const FrameLimiter&) = delete;

        /**
         * At most t_Fps frames a second; 0 for no cap. A new rate starts the schedule again at
         * the next frame; the rate it has already changes nothing.
         */
        CORE_API void SetMaxFps(uint32 t_Fps);

        /** The cap in frames a second; 0 for no cap. */
        uint32 GetMaxFps() const { return m_MaxFps; }

        /** The frame length of the cap in nanoseconds; 0 for no cap. */
        int64 GetPeriodNs() const { return m_PeriodNs; }

        /** How long before a deadline the wait stops sleeping and spins. At most half a period. */
        CORE_API int64 GetSpinNs() const;

        /**
         * Wait until the next frame is due. The first call after SetMaxFps starts the schedule
         * and does not wait. With no cap it returns at once and reads no clock.
         */
        CORE_API void WaitForNextFrame();

        /** The number of times the schedule started again because a frame was too late. */
        uint64 GetRestarts() const { return m_Restarts; }

    private:
        /** Owned by the steady-clock limiter: its sleep. */
        std::unique_ptr<SleepTimer> m_Timer;

        NowFunction m_Now;
        SleepFunction m_Sleep;
        int64 m_SpinNs = 0;

        uint32 m_MaxFps = 0;
        int64 m_PeriodNs = 0;
        int64 m_NextNs = 0;
        bool m_Started = false;
        uint64 m_Restarts = 0;
    };
} // namespace Fling
