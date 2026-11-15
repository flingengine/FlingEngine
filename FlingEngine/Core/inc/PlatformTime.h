#pragma once

#include "CoreAPI.h"
#include "FlingTypes.h"

#include <atomic>

namespace Fling::PlatformTime
{
    /** The length of one period at t_PerSecond a second, in whole nanoseconds (rounded): 33 333 333 at 30. 0 for 0. */
    constexpr int64 PeriodNs(const uint32 t_PerSecond)
    {
        return t_PerSecond == 0 ? 0 : (int64(1'000'000'000) + int64(t_PerSecond) / 2) / int64(t_PerSecond);
    }

    /**
     * A clock that code sets by hand. Headless runs, replays, and tests install one
     * (UseVirtualClock), so every input timestamp and every tick window is the same on
     * every run, whatever the speed of the machine.
     *
     * One writer: only one thread calls Set or Advance (Advance reads, then sets, so two
     * writers could lose a step). Any thread may read it.
     */
    class CORE_API VirtualClock
    {
    public:
        explicit VirtualClock(const int64 t_StartNs = 0) : m_NowNs(t_StartNs) {}

        int64 Now() const { return m_NowNs.load(std::memory_order_relaxed); }

        /** Set the time. It must not go back: input message times never decrease. */
        void Set(int64 t_NowNs);

        void Advance(const int64 t_DeltaNs) { Set(Now() + t_DeltaNs); }

    private:
        std::atomic<int64> m_NowNs;
    };

    /**
     * The engine's one clock, in nanoseconds: input message times, tick windows, frame
     * times, and the tick ledger. It is monotonic (steady_clock), or the installed
     * VirtualClock. Simulation code must not read it: a debug build asserts while the
     * runner runs fixed rate systems (SimulationCheck). Use SimTime instead.
     */
    CORE_API int64 NowNanoseconds();

    /** The steady clock, also while a virtual clock is installed. For logs and profiling only. */
    CORE_API int64 SteadyNanoseconds();

    /**
     * Make NowNanoseconds read t_Clock (null: the steady clock again). The clock must
     * outlive the time it is installed. One clock for the whole process.
     */
    CORE_API void UseVirtualClock(const VirtualClock* t_Clock);

    /** The installed virtual clock, or null. */
    CORE_API const VirtualClock* GetVirtualClock();

    /** Installs a virtual clock for a scope (a test), and puts back the one before. */
    class CORE_API ScopedVirtualClock
    {
    public:
        explicit ScopedVirtualClock(int64 t_StartNs = 0);
        ~ScopedVirtualClock();

        ScopedVirtualClock(const ScopedVirtualClock&) = delete;
        ScopedVirtualClock& operator=(const ScopedVirtualClock&) = delete;

        VirtualClock& Clock() { return m_Clock; }

    private:
        VirtualClock m_Clock;
        const VirtualClock* m_Previous = nullptr;
    };
} // namespace Fling::PlatformTime
