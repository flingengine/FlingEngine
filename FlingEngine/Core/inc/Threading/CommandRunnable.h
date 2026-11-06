#pragma once

#include "Threading/CommandQueue.h"
#include "Threading/Runnable.h"
#include "Threading/TickLimiter.h"

#include <atomic>
#include <chrono>
#include <optional>
#include <utility>

namespace Fling
{
    /**
     * A Runnable that runs the commands that other threads send to it. This is
     * the base of a subsystem thread, for example audio.
     *
     * There are two modes:
     *
     * - Wake interval (the default). Each loop waits for a command, Stop, or the
     *   wake interval, then runs the commands, then calls Tick. A command runs
     *   soon after it is sent.
     * - Fixed rate (construct with a TickRate). Each loop waits for the next tick
     *   of the rate (see TickLimiter), then runs the commands, then calls Tick.
     *   Commands run only at the start of a tick, so every command sent before a
     *   tick is seen by that tick.
     *
     * After Stop, Run runs the commands that are left and returns. A command sent
     * after Run returns does not run.
     *
     * Without a thread (tests, --null-render), the owner calls ExecuteCommands
     * and Tick from its own loop. The same code then runs on one thread.
     */
    class CommandRunnable : public Runnable
    {
    public:
        explicit CommandRunnable(std::chrono::nanoseconds t_WakeInterval = std::chrono::milliseconds(100))
            : m_WakeInterval(t_WakeInterval)
        {
        }

        /** Tick at t_Rate. A TickRate that is not fixed gives the default wake interval mode. */
        explicit CommandRunnable(TickRate t_Rate) : m_WakeInterval(std::chrono::milliseconds(100))
        {
            if (t_Rate.IsFixed())
            {
                m_Limiter.emplace(t_Rate);
            }
        }

        /** Send t_Command to this runnable. Any thread can call this. */
        void Enqueue(Command t_Command) { m_Commands.Push(std::move(t_Command)); }

        /** Run the commands that are in the queue now, on the calling thread. */
        uint32 ExecuteCommands() { return m_Commands.Execute(); }

        bool IsStopRequested() const { return m_StopRequested.load(std::memory_order_acquire); }

        /** The fixed rate, or a TickRate that is not fixed in wake interval mode. */
        TickRate GetTickRate() const { return m_Limiter ? m_Limiter->GetRate() : TickRate{}; }

        /** Ticks dropped because a Tick took too long. Always 0 in wake interval mode. */
        uint64 GetMissedTicks() const { return m_Limiter ? m_Limiter->GetMissedTicks() : 0; }

        uint32 Run() override
        {
            while (!IsStopRequested())
            {
                if (m_Limiter)
                {
                    if (!m_Limiter->WaitForNextTick())
                    {
                        break;
                    }
                }
                else
                {
                    m_Commands.WaitForItems(m_WakeInterval);
                }
                m_Commands.Execute();
                Tick();
            }

            // Commands sent before Stop still run
            m_Commands.Execute();
            return 0;
        }

        void Stop() override
        {
            m_StopRequested.store(true, std::memory_order_release);
            m_Commands.Wake();
            if (m_Limiter)
            {
                m_Limiter->Interrupt();
            }
        }

        /** The work of one loop, after the commands. For example, mix the next audio buffer. */
        virtual void Tick() {}

    private:
        CommandQueue m_Commands;
        std::atomic<bool> m_StopRequested{false};
        const std::chrono::nanoseconds m_WakeInterval;
        std::optional<TickLimiter> m_Limiter;
    };
} // namespace Fling
