#pragma once

#include "FlingTypes.h"
#include "PlatformTime.h"

namespace Fling
{
    /**
     * The fixed simulation rate in ticks a second ([Engine] SimHz, --sim-hz). The one
     * place that turns the rate into the tick length: SystemRunnerConfig::SimHz feeds the
     * tick timeline and SimTime::FixedDeltaNs, and the physics step takes that delta. The
     * rate changes only at a tick boundary, through a SetRate clock command
     * (Docs/time-and-ticks.md section 2, "Changing the rate while the game runs").
     */
    namespace SimRate
    {
        /** The rate when neither the config file nor the command line sets one. */
        constexpr uint32 DefaultHz = 30;

        /** The lowest and the highest rate that a setting may ask for. */
        constexpr uint32 MinHz = 1;
        constexpr uint32 MaxHz = 1000;

        /** True for a rate from MinHz to MaxHz. */
        constexpr bool IsValidHz(const uint32 t_Hz)
        {
            return t_Hz >= MinHz && t_Hz <= MaxHz;
        }

        /** The length of one tick at t_Hz, in whole nanoseconds (rounded): 33 333 333 at 30 Hz. */
        constexpr int64 StepNs(const uint32 t_Hz)
        {
            return PlatformTime::PeriodNs(t_Hz);
        }
    } // namespace SimRate
} // namespace Fling
