#include "Platform.h"
#include "Threading/TickLimiter.h"

#include <algorithm>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#elif defined(_M_ARM64)
#include <intrin.h>
#endif

#if FLING_WINDOWS
// Windows 10 1803 and later. MinGW headers can be older.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif

namespace Fling
{
    namespace
    {
        using namespace std::chrono_literals;

#if FLING_WINDOWS
        // Without the high-resolution timer, a Windows sleep wakes up to one
        // system tick (15.6 ms) late, so the spin window must be at least that
        constexpr std::chrono::nanoseconds LowResolutionSpinWindow = 16ms;
#endif

        /**
         * Tell the CPU that this is a spin loop, which eases the spin on the other
         * hyperthread of the core. Do not give the core to the scheduler here
         * (std::this_thread::yield, SwitchToThread). With every core busy, that
         * can delay the return by a full scheduling quantum, which is milliseconds.
         */
        void CpuRelax()
        {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
            _mm_pause();
#elif defined(_M_ARM64)
            __yield();
#elif defined(__aarch64__) || defined(__arm__)
            asm volatile("yield");
#endif
        }
    } // namespace

    TickLimiter::TickLimiter(TickRate t_Rate) : m_Rate(t_Rate)
    {
#if FLING_WINDOWS
        if (m_Rate.IsFixed())
        {
            m_NativeTimer =
                CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
            if (m_NativeTimer != nullptr)
            {
                // Manual reset, so that the interrupt stays set
                m_NativeInterrupt = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                if (m_NativeInterrupt == nullptr)
                {
                    CloseHandle(m_NativeTimer);
                    m_NativeTimer = nullptr;
                }
            }
        }
#endif

        if (m_Rate.IsFixed() && m_Rate.IsPrecise())
        {
            std::chrono::nanoseconds Window = std::max(m_Rate.SpinWindow, std::chrono::nanoseconds(0));
#if FLING_WINDOWS
            if (m_NativeTimer == nullptr)
            {
                Window = std::max(Window, LowResolutionSpinWindow);
            }
#endif
            m_SpinMargin = std::min(Window, m_Rate.Period / 2);
        }
    }

    TickLimiter::~TickLimiter()
    {
#if FLING_WINDOWS
        if (m_NativeTimer != nullptr)
        {
            CloseHandle(m_NativeTimer);
        }
        if (m_NativeInterrupt != nullptr)
        {
            CloseHandle(m_NativeInterrupt);
        }
#endif
    }

    bool TickLimiter::WaitForNextTick()
    {
        if (IsInterrupted())
        {
            return false;
        }
        if (!m_Rate.IsFixed())
        {
            return true;
        }

        const Clock::time_point Now = Clock::now();
        if (!m_Started)
        {
            m_Started = true;
            m_LastTickStart = Now;
            m_NextTick = Now + m_Rate.Period;
        }
        else if (m_NextTick < m_LastTickStart + m_Rate.Period / 2)
        {
            // The last tick started more than half a period late. Start the
            // schedule again from that tick, so that the next ticks do not run
            // back to back to catch up. The ticks of the old schedule that are
            // before the new tick are dropped.
            const Clock::time_point Restart = m_LastTickStart + m_Rate.Period;
            const auto Dropped = (Restart - m_NextTick + m_Rate.Period - Clock::duration(1)) / m_Rate.Period;
            m_MissedTicks.fetch_add(static_cast<uint64>(Dropped), std::memory_order_relaxed);
            m_NextTick = Restart;
        }

        if (Now < m_NextTick)
        {
            // m_SpinMargin is 0 when not precise, so this sleeps until the tick
            const Clock::time_point WakeAt = m_NextTick - m_SpinMargin;
            if (Now < WakeAt && !WaitUntil(WakeAt))
            {
                return false;
            }
            if (m_Rate.IsPrecise() && !SpinUntil(m_NextTick))
            {
                return false;
            }
        }
        // When the tick is already due (the last Tick took too long), it runs now.
        // The next call starts the schedule again when this start was too late.

        m_LastTickStart = Clock::now();
        m_NextTick += m_Rate.Period;
        return !IsInterrupted();
    }

    void TickLimiter::Interrupt()
    {
        {
            std::lock_guard<std::mutex> Lock(m_Mutex);
            m_Interrupted.store(true, std::memory_order_release);
        }
        m_Condition.notify_all();
#if FLING_WINDOWS
        if (m_NativeInterrupt != nullptr)
        {
            SetEvent(m_NativeInterrupt);
        }
#endif
    }

    bool TickLimiter::SpinUntil(Clock::time_point t_Deadline) const
    {
        while (Clock::now() < t_Deadline)
        {
            if (IsInterrupted())
            {
                return false;
            }
            CpuRelax();
        }
        return !IsInterrupted();
    }

    bool TickLimiter::WaitUntil(Clock::time_point t_Deadline)
    {
#if FLING_WINDOWS
        if (m_NativeTimer != nullptr)
        {
            // A wake can come before the deadline (a rounded due time), so loop
            for (Clock::time_point Now = Clock::now(); Now < t_Deadline; Now = Clock::now())
            {
                // A negative due time is relative, in 100 ns units
                const auto Remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(t_Deadline - Now);
                LARGE_INTEGER DueTime;
                DueTime.QuadPart = -static_cast<LONGLONG>((Remaining.count() + 99) / 100);
                if (!SetWaitableTimerEx(m_NativeTimer, &DueTime, 0, nullptr, nullptr, nullptr, 0))
                {
                    break;
                }

                const HANDLE Handles[2] = {m_NativeTimer, m_NativeInterrupt};
                const DWORD Result = WaitForMultipleObjects(2, Handles, FALSE, INFINITE);
                if (Result != WAIT_OBJECT_0)
                {
                    return !IsInterrupted();
                }
            }
            if (Clock::now() >= t_Deadline)
            {
                return !IsInterrupted();
            }
            // SetWaitableTimerEx failed: wait on the condition variable instead
        }
#endif
        std::unique_lock<std::mutex> Lock(m_Mutex);
        m_Condition.wait_until(Lock, t_Deadline, [this]() { return IsInterrupted(); });
        return !IsInterrupted();
    }
} // namespace Fling
