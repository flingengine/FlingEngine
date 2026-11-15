#pragma once

#include "FlingTypes.h"
#include "Threading/Lane.h"

namespace Fling
{
    /**
     * What a TickDriver drives: one world's wakes. A wake applies the clock commands, runs
     * the fixed ticks that are due on the host's clock, and after each tick publishes it and
     * sends its outbox entry (World::RunWake). The host reads its own clock.
     *
     * Every function is for the thread that runs the wakes.
     */
    class TickHost
    {
    public:
        /** Run one wake. Returns the ticks that ran. */
        virtual uint32 RunWake() = 0;

        /** Nanoseconds on the host's clock until the next tick is due. 0 or less: due now. */
        virtual int64 NanosecondsToNextTick() const = 0;

        /** True while a Pause command holds the ticks. */
        virtual bool IsTickClockPaused() const = 0;

        /** True after the last tick of the session ran (the stop tick, or a Quit): no wake runs a tick again. */
        virtual bool HasRunLastTick() const = 0;

    protected:
        ~TickHost() = default;
    };

    /**
     * Which thread runs a world's wakes (Docs/threading-model.md section 4). The world picks
     * one at play start and keeps it until play stops: SameThreadDriver (Edit, Direct and
     * Inline play) or GameLane (Threaded play). Every function is for the main thread,
     * except ShouldStopWaiting.
     */
    class TickDriver
    {
    public:
        virtual ~TickDriver() = default;

        /** Once per frame, before the frame side: SameThreadDriver runs one wake here. GameLane does nothing. */
        virtual void OnFrame() = 0;

        /** A clock command was sent: a lane that sleeps while paused wakes to apply it. */
        virtual void OnClockCommand() {}

        /**
         * Any thread: true when a tick that waits for the input writer (the main thread) must
         * stop waiting, because the main thread waits for the tick thread to stop. Always true
         * when the wakes run on the writer's thread.
         */
        virtual bool ShouldStopWaiting() const = 0;
    };

    /** The wakes run on the main thread, in the frame, before the frame side (Direct and Inline play, and Edit). */
    class SameThreadDriver final : public TickDriver
    {
    public:
        /** t_TickLane: the lane of the ticks, which owns the simulation storage (Game in Inline play). */
        SameThreadDriver(TickHost& t_Host, const Lane t_TickLane) : m_Host(t_Host), m_TickLane(t_TickLane) {}

        void OnFrame() override
        {
            const LaneCheck::Scope TickLane(m_TickLane);
            m_Host.RunWake();
        }

        bool ShouldStopWaiting() const override { return true; }

    private:
        TickHost& m_Host;
        const Lane m_TickLane;
    };
} // namespace Fling
