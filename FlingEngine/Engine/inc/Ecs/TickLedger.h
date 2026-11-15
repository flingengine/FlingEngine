#pragma once

#include "Ecs/TickStamp.h"
#include "FlingTypes.h"
#include "TickHistory.h"

namespace Fling
{
    /**
     * What one simulation tick was: its stamp, and how it ran. The runner writes one at
     * the end of each tick.
     *
     * Stamp.Tick, Stamp.Cut, Stamp.StateHash and the input counts are the same for the
     * same input at any frame rate: compare them across runs (--dump-ticks). The window
     * and RanAtNs are wall clock times (the platform clock): use them for latency and
     * pacing, never for results.
     */
    struct TickRecord
    {
        TickStamp Stamp;

        /** The platform clock when the tick started to run. Later than the window's end for a TickTrigger::Clock tick.
         */
        int64 RanAtNs = 0;

        /** Tick lengths of time skipped right before this tick (a hitch over SystemRunnerConfig::MaxCatchUp). No tick
         * number is skipped. */
        uint64 DroppedSteps = 0;

        /** The mouse move and key edge messages of the tick (TickInput::Slice). */
        uint32 MouseSamples = 0;
        uint32 KeyEvents = 0;

        /**
         * The input queue overwrote messages before this tick took them (InputSlice::Overran):
         * the tick lost input, so a run with it is not reproducible from its input.
         */
        bool InputOverran = false;
    };

    /**
     * The last 256 tick records. An engine object that the runner owns and writes at the
     * end of each tick; it is not in the ECS, so no fixed system can read its wall clock
     * times. Presentation code gets the latest stamp through RealFrameTime, and tools the
     * records through World::TakeNewTickRecords.
     */
    using TickLedger = TickHistory<TickRecord, 256>;
} // namespace Fling
