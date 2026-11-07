#pragma once

#include "Ecs/Component.h"
#include "FlingTypes.h"

namespace Fling
{
    /**
     * The simulation clock. The runner writes it before each fixed tick, and no
     * system may write it. Fixed rate systems use it for their delta time.
     */
    struct SimTime
    {
        static constexpr ComponentInfo Component{
            .Name = "SimTime", .Kind = ComponentKind::Singleton, .Transient = true};

        /** The number of the tick that is running. The first tick is 1. */
        uint64 Tick = 0;
        /** The length of one tick in seconds. It never changes while the world runs. */
        float FixedDelta = 1.0f / 60.0f;
        /** Tick * FixedDelta, in seconds. */
        double Elapsed = 0.0;
    };

    /**
     * The real time of this rendered frame. The runner writes it before the frame
     * phases. It is a Presentation component, so a fixed rate system cannot read
     * it: the simulation must not depend on the frame rate.
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
        /** How far real time is between the last tick and the next, from 0 to 1. Use it to interpolate visuals. */
        float TickAlpha = 0.0f;
    };
} // namespace Fling
