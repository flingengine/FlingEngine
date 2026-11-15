#pragma once

#include "Components/AppRequests.h"
#include "FlingTypes.h"

#include <vector>

namespace Fling
{
    /**
     * What one tick sent out of the simulation to the application (the outbox,
     * Docs/architecture-overview.md section 5): its AppRequests, with the tick that made them. The tick's thread pushes
     * one entry per tick that made requests; the main thread applies each entry once. An entry whose tick is not newer
     * than the last applied one is a repeat (a resimulated tick) and is dropped.
     */
    struct TickOutboxEntry
    {
        uint64 Tick = 0;
        std::vector<AppRequest> Requests;
    };

    /** A change to the tick clock that the main thread asks for. */
    enum class ClockCommandType : uint8
    {
        /** No tick runs until Resume. The windows of the ticks that have not run move by the pause. */
        Pause,
        Resume,

        /**
         * The simulation rate is ClockCommand::SimHz from the next tick on: its window, every
         * later window and SimTime::FixedDeltaNs have the new length (SystemRunner::SetSimHz).
         */
        SetRate,
    };

    /**
     * A clock command, stamped with the tick it goes before. The tick's thread applies it
     * at the start of a wake, between two ticks, in every exec mode.
     *
     * BeforeTick 0: apply it at the next wake (live play; the main thread cannot know which
     * tick the game thread is at). BeforeTick N: apply it right before tick N (a replay or a
     * test that puts it at a known boundary); the wake runs no tick past N - 1 while it waits.
     * A stamped command whose tick has already run is a repeat and is dropped.
     */
    struct ClockCommand
    {
        ClockCommandType Type = ClockCommandType::Pause;
        uint64 BeforeTick = 0;

        /** SetRate only: the new rate in ticks a second (SimRate::IsValidHz). */
        uint32 SimHz = 0;
    };
} // namespace Fling
