#pragma once

#include "CoreAPI.h"
#include "FlingTypes.h"

namespace Fling
{
    /** Why a tick ran. It decides which input the tick takes (TickInputProvider). */
    enum class TickTrigger : uint8
    {
        /**
         * The clock reached the end of the tick's window (SystemRunner::RunFixedAt). The
         * tick takes the messages with a time before the end of its window.
         */
        Clock,

        /** The tick ran on request (RunTick, OneTickPerFrame). It takes every message in the queue. */
        Request,
    };

    /**
     * The window of one simulation tick on the platform clock (PlatformTime): the tick
     * covers [StartNs, EndNs). The tick number is not part of it: TickTimeline::NextTick
     * and TickStamp::Tick hold it. Wall clock times: for input assignment, pacing and
     * presentation, never for simulation results.
     */
    struct TickWindow
    {
        int64 StartNs = 0;
        int64 EndNs = 0;
        TickTrigger Trigger = TickTrigger::Request;

        bool operator==(const TickWindow&) const = default;
    };

    /**
     * Maps tick numbers to windows on the platform clock. Tick n starts at
     * BaseNs + (n - BaseTick) * StepNs, so nothing is summed. The next tick number only
     * ever goes up by one: a hitch, a pause or a new step moves the windows of the ticks
     * that have not run (a rebase), never the numbers.
     *
     * Rules:
     * - Start(t): the first tick's window starts at t.
     * - A tick is due when the clock reaches the end of its window.
     * - Hitch: when more ticks are due than a frame may run, DropBacklog moves the
     *   windows forward by whole ticks, so the next window is the one that holds now.
     *   No tick number is skipped; the time is.
     * - Pause at p, resume at r: the windows of the ticks that have not run move by
     *   r - p. No tick is due while paused.
     * - SetStep: the next tick starts where the last one ended and has the new length.
     */
    class CORE_API TickTimeline
    {
    public:
        explicit TickTimeline(int64 t_StepNs);

        int64 StepNs() const { return m_StepNs; }

        bool IsStarted() const { return m_Started; }

        /** The first tick that has not run gets the window [t_StartNs, t_StartNs + step). */
        void Start(int64 t_StartNs);

        /** The number of the tick that runs next. The first tick is 1. */
        uint64 NextTick() const { return m_NextTick; }

        /** The window of the next tick, with TickTrigger::Request. Only valid after Start. */
        TickWindow NextWindow() const;

        /** True if started, not paused, and the clock has reached the end of the next window. */
        bool IsDue(int64 t_NowNs) const;

        /** The next tick runs now: returns its window and moves to the tick after it. */
        TickWindow Advance(TickTrigger t_Trigger);

        /**
         * After a frame ran as many ticks as it may: move the windows forward by whole
         * ticks until the next one ends after t_NowNs. Returns the number of tick lengths
         * dropped (0 if none was due).
         */
        uint64 DropBacklog(int64 t_NowNs);

        void Pause(int64 t_NowNs);

        /** The windows of the ticks that have not run move by the time of the pause. */
        void Resume(int64 t_NowNs);

        bool IsPaused() const { return m_Paused; }

        /**
         * A new tick length from the next tick on (a simulation rate change at a tick
         * boundary): the next window starts where it started before, and it and every later
         * one are t_StepNs long.
         */
        void SetStep(int64 t_StepNs);

    private:
        int64 StartOf(uint64 t_Tick) const { return m_BaseNs + static_cast<int64>(t_Tick - m_BaseTick) * m_StepNs; }

        int64 m_StepNs = 0;
        uint64 m_BaseTick = 1;
        int64 m_BaseNs = 0;
        uint64 m_NextTick = 1;
        int64 m_PausedAtNs = 0;
        bool m_Started = false;
        bool m_Paused = false;
    };
} // namespace Fling
