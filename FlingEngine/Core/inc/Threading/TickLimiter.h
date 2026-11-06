#pragma once

#include "CoreAPI.h"
#include "Threading/ConditionVariable.h"
#include "Threading/Mutex.h"
#include "Threading/ThreadTypes.h"

#include <atomic>
#include <chrono>

namespace Fling
{
    /**
     * Wait for each tick of a fixed rate, for a thread that must run at, for
     * example, 60 or 120 Hz.
     *
     * The ticks stay on a fixed schedule: tick N is due at start + N * Period, so
     * a late wake does not move the next ticks. When a Tick takes too long, the
     * next tick runs at once. When a tick starts more than half a period late,
     * the schedule starts again from that tick, so ticks do not run back to back
     * to catch up. The dropped ticks are counted in GetMissedTicks.
     *
     * A wake is never early. With TickPrecision::Normal it can be late by the OS
     * timer resolution (about 0.1 ms on Linux, about 0.5 ms with the Windows
     * high-resolution timer). With TickPrecision::Precise the limiter wakes
     * GetSpinMargin before the tick and spins until the tick, so it is late by
     * only a few microseconds, at the cost of CPU time.
     *
     * #TODO: The sleep-then-spin wait (WaitUntil + SpinUntil) could become a
     * general Timer type later, for code that must wait until an exact time
     * but has no fixed rate. It is not needed yet.
     *
     * Only one thread calls WaitForNextTick. Any thread can call Interrupt.
     */
    class TickLimiter
    {
    public:
        CORE_API explicit TickLimiter(TickRate t_Rate);

        CORE_API ~TickLimiter();

        TickLimiter(const TickLimiter&) = delete;
        TickLimiter& operator=(const TickLimiter&) = delete;
        TickLimiter(TickLimiter&&) = delete;
        TickLimiter& operator=(TickLimiter&&) = delete;

        /**
         * Wait until the next tick is due. The first call starts the schedule, and
         * its tick is due one Period later. With no fixed rate, return at once.
         *
         * @return False when Interrupt was called
         */
        CORE_API bool WaitForNextTick();

        /** Make WaitForNextTick return false, now and on each later call. */
        CORE_API void Interrupt();

        bool IsInterrupted() const { return m_Interrupted.load(std::memory_order_acquire); }

        TickRate GetRate() const { return m_Rate; }

        /** The number of ticks that were dropped because the caller was late. */
        uint64 GetMissedTicks() const { return m_MissedTicks.load(std::memory_order_relaxed); }

        /** How long before each tick a precise limiter stops its sleep and spins. 0 when not precise. */
        std::chrono::nanoseconds GetSpinMargin() const { return m_SpinMargin; }

    private:
        using Clock = std::chrono::steady_clock;

        /** Wait until t_Deadline or Interrupt. Return false on Interrupt. */
        bool WaitUntil(Clock::time_point t_Deadline);

        /** Spin until t_Deadline or Interrupt. Return false on Interrupt. */
        bool SpinUntil(Clock::time_point t_Deadline) const;

        const TickRate m_Rate;
        std::chrono::nanoseconds m_SpinMargin{0};
        Clock::time_point m_NextTick{};
        Clock::time_point m_LastTickStart{};
        bool m_Started = false;

        std::atomic<bool> m_Interrupted{false};
        std::atomic<uint64> m_MissedTicks{0};
        Mutex m_Mutex;
        ConditionVariable m_Condition;

        // A high-resolution waitable timer and an interrupt event. Null on Windows
        // versions before 10 1803 (the limiter then waits on m_Condition). _WIN32
        // and not FLING_WINDOWS, so that this header does not include Windows.h.
#if defined(_WIN32)
        void* m_NativeTimer = nullptr;
        void* m_NativeInterrupt = nullptr;
#endif
    };
} // namespace Fling
