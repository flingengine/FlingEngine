#pragma once

#include "Ecs/Component.h"
#include "Ecs/TickStamp.h"
#include "FlingTypes.h"

#include <cstddef>

namespace Fling
{
    /**
     * The simulation clock. The runner writes it before each fixed tick, and no
     * system may write it. Fixed rate systems use it for their delta time.
     *
     * It holds only what a fixed system may depend on: the tick number, the tick's length
     * and the sim time at its end. It has no wall time, no tick window, no pause flag and no
     * frame rate, so a tick gives the same result however late it ran. A rollback does not
     * restore it: the runner writes it from each tick's stamp (SimTimeOf), which the ledger
     * keeps.
     */
    struct SimTime
    {
        static constexpr ComponentInfo Component{
            .Name = "SimTime", .Kind = ComponentKind::Singleton, .Transient = true, .Rollback = RollbackClass::Rebuild};

        /** The number of the tick that is running. The first tick is 1. */
        uint64 Tick = 0;

        /**
         * The length of this tick in nanoseconds: the runner writes it from the simulation
         * rate ([Engine] SimHz, or a SetRate clock command that changed it at a tick boundary).
         */
        int64 FixedDeltaNs = 0;

        /**
         * The sim time at the end of this tick: Tick * FixedDeltaNs while the rate never changed;
         * after a change, the sim time at the last tick of the old rate plus the ticks since at
         * the new one. Computed from the tick number, never a running sum (SystemRunner).
         */
        int64 ElapsedNs = 0;

        /** The length of one tick in seconds, for the delta time of a fixed system. */
        float DeltaSeconds() const { return static_cast<float>(static_cast<double>(FixedDeltaNs) * 1e-9); }

        /** ElapsedNs in seconds. */
        double ElapsedSeconds() const { return static_cast<double>(ElapsedNs) * 1e-9; }

        bool operator==(const SimTime&) const = default;
    };
    // The same layout on every platform: a build where this fails has drifted from the others
    static_assert(sizeof(SimTime) == 24 && offsetof(SimTime, FixedDeltaNs) == 8 && offsetof(SimTime, ElapsedNs) == 16);

    /** The SimTime of a tick, from its stamp alone (the ledger keeps the stamps). */
    inline SimTime SimTimeOf(const TickStamp& t_Stamp)
    {
        return SimTime{.Tick = t_Stamp.Tick, .FixedDeltaNs = t_Stamp.FixedDeltaNs, .ElapsedNs = t_Stamp.ElapsedNs};
    }

    /**
     * The real time of this rendered frame. The runner writes it before the frame
     * groups. It is a Presentation component, so a fixed rate system cannot read
     * it: the simulation must not depend on the frame rate.
     *
     * Latest is the stamp of the tick whose state the frame shows: the ledger's newest
     * record with one storage, the header of the packet in the mirror with two. So the
     * tick, its window and its input cut always belong to the state the frame reads.
     */
    struct RealFrameTime
    {
        static constexpr ComponentInfo Component{.Name = "RealFrameTime",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Domain = ComponentDomain::Presentation,
                                                 .Transient = true};

        /** Seconds since the last frame. */
        float DeltaSeconds = 0.0f;
        /** Seconds since the runner was made. */
        double Elapsed = 0.0;
        /**
         * The blend factor of this frame, from 0 to 1: draw ctx.BlendFrom<T> (the BlendFrom
         * tick's values) blended toward ctx.BlendTo<T> (the BlendTo tick's) by it. It is the
         * renderer's own, on the frame clock (the rule is in FrameBlend).
         */
        float BlendAlpha = 1.0f;
        /** The platform clock time of this frame (PlatformTime, or the runner's clock). */
        int64 FrameNs = 0;
        /**
         * The newest tick the frame side holds (Latest.Tick 0 before the first): the state
         * that Read<T> gives, and the input cut that PendingInput follows.
         */
        TickStamp Latest;
        /**
         * The tick at factor 0: the values that ctx.BlendFrom<T> gives. In Play, the tick
         * right before BlendTo (the published tick N - 1); BlendTo itself when there is none
         * (the start of play) or with interpolation off.
         */
        TickStamp BlendFrom;
        /** The tick at factor 1: the values that ctx.BlendTo<T> gives. In Play, always Latest. */
        TickStamp BlendTo;
    };
} // namespace Fling
