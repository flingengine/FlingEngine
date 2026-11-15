#pragma once

#include "FlingTypes.h"
#include "Input/InputCut.h"
#include "TickTimeline.h"

namespace Fling
{
    /**
     * The identity of one simulation tick: its number, its length and sim time, its window,
     * the input it consumed, and the state it made. Everything that names a tick holds one:
     * the ledger record, the published packet header, and the presentation copy
     * (RealFrameTime::Latest). Tick is the key; the other fields describe that tick.
     *
     * Tick, FixedDeltaNs, ElapsedNs, Cut and StateHash are the same for the same input at
     * any frame rate. Window is wall clock time (the platform clock): for input assignment,
     * pacing and presentation, never for simulation results.
     */
    struct TickStamp
    {
        /** The tick number. The first tick is 1; 0 means no tick has run. */
        uint64 Tick = 0;

        /**
         * The tick's length and the sim time at its end: with Tick, all of the tick's SimTime
         * (SimTimeOf). The runner writes SimTime from them, so a tick can be run again from
         * its stamp alone, also after a rate change.
         */
        int64 FixedDeltaNs = 0;
        int64 ElapsedNs = 0;

        /** The tick's window on the platform clock, and why it ran. */
        TickWindow Window;

        /** The input messages the tick consumed. */
        InputCut Cut;

        /** HashSimulationState after the tick (0 if the runner has no hasher). */
        uint64 StateHash = 0;

        bool operator==(const TickStamp&) const = default;
    };
} // namespace Fling
