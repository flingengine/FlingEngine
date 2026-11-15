#include "GameLane.h"
#include "Logger.h"
#include "MathUtils.h"
#include "Threading/Lane.h"
#include "Threading/Thread.h"

#include <cassert>
#include <chrono>
#include <thread>

namespace Fling
{
    namespace
    {
        /**
         * The longest sleep between two wakes. The wait is computed on the host's clock and
         * slept on the steady clock, so a host clock that is not the steady clock (a virtual
         * one) is read again at least this often.
         */
        constexpr std::chrono::milliseconds LongestWait{100};

        /**
         * The lane waits for the host's next tick (WaitUntil), never on the limiter's own
         * schedule. A fixed period only makes the limiter use the high-resolution timer, so
         * it is not a tick rate.
         */
        constexpr TickRate WaiterRate{.Period = LongestWait};

        /** How often Step looks whether the lane finished while it waits for a wake. */
        constexpr std::chrono::milliseconds StepPoll{1};
    } // namespace

    GameLane::GameLane(TickHost& t_Host, const GameLaneDesc& t_Desc)
        : m_Host(t_Host),
          m_Desc(t_Desc),
          m_Waiter(WaiterRate),
          m_Jitter(t_Desc.JitterSeed, t_Desc.JitterMaxMicroseconds)
    {
    }

    GameLane::~GameLane()
    {
        if (!m_Thread)
        {
            return;
        }
        m_StopRequested.store(true, std::memory_order_release);
        m_Waiter.Interrupt();
        m_WakeUp.Signal();
        m_Permits.Release();
        m_Thread->Join();
        m_Thread.reset();
    }

    bool GameLane::Start()
    {
        assert(!m_Thread);
        m_Thread = Thread::Create(ThreadDesc{.Name = "Game Thread"}, [this]() { Run(); });
        if (!m_Thread)
        {
            F_LOG_ERROR("Cannot start the game thread");
            return false;
        }
        return true;
    }

    void GameLane::Step(const uint32 t_Wakes)
    {
        assert(m_Desc.Pacing == GameLanePacing::Stepped && "Step is for Stepped pacing");
        m_Permits.Release(t_Wakes);
        for (uint32 i = 0; i < t_Wakes; ++i)
        {
            while (!m_WakesDone.TryAcquireFor(StepPoll))
            {
                if (IsFinished())
                {
                    return;
                }
            }
        }
    }

    void GameLane::Run()
    {
        const LaneCheck::Scope GameLaneScope(Lane::Game);
        while (!IsStopRequested() && WaitForPermit())
        {
            const uint32 Ticks = Wake();
            if (m_Desc.Pacing == GameLanePacing::Stepped)
            {
                m_WakesDone.Release();
            }
            if (m_Host.HasRunLastTick())
            {
                F_LOG_TRACE("Game thread: the last tick of the session ran");
                break;
            }
            WaitForNextWake(Ticks);
        }
        m_Finished.store(true, std::memory_order_release);
    }

    bool GameLane::WaitForPermit()
    {
        if (m_Desc.Pacing != GameLanePacing::Stepped)
        {
            return true;
        }
        m_Permits.Acquire();
        return !IsStopRequested();
    }

    uint32 GameLane::Wake()
    {
        m_Jitter.Sleep();
        const uint32 Ticks = m_Host.RunWake();
        m_Wakes.fetch_add(1, std::memory_order_acq_rel);
        if (Ticks > 0)
        {
            m_Jitter.Sleep();
        }
        return Ticks;
    }

    void GameLane::WaitForNextWake(const uint32 t_TicksRun)
    {
        if (m_Desc.Pacing == GameLanePacing::Stepped)
        {
            return;
        }
        if (m_Host.IsTickClockPaused())
        {
            // No tick can be due until a Resume: sleep until a clock command or the stop
            m_WakeUp.Wait();
            return;
        }
        if (m_Desc.Pacing == GameLanePacing::AsFastAsPossible)
        {
            if (t_TicksRun == 0)
            {
                std::this_thread::yield();
            }
            return;
        }
        const int64 WaitNs = m_Host.NanosecondsToNextTick();
        if (WaitNs > 0)
        {
            const std::chrono::nanoseconds Wait =
                Math::Min<std::chrono::nanoseconds>(std::chrono::nanoseconds(WaitNs), LongestWait);
            m_Waiter.WaitUntil(std::chrono::steady_clock::now() + Wait);
        }
    }
} // namespace Fling
