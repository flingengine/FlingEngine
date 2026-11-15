#pragma once

#include "Ecs/Component.h"
#include "FlingTypes.h"
#include "LookCamera.h"

#include <algorithm>
#include <cstddef>

/**
 * Stasis: a gameplay effect that stops the look camera for a while. It is on the camera
 * entity. Sandbox.Stasis starts it on a press of G, counts it down in simulation time, and
 * sets LookCamera::Policy.InStasis while it lasts. Game state: a rollback restores it, and
 * the state hash covers it. Not saved to a level.
 */
struct Stasis
{
    static constexpr Fling::ComponentInfo Component{
        .Name = "Stasis", .Transient = true, .Rollback = Fling::RollbackClass::Restore};

    /** Simulation time left, in nanoseconds. 0: not in stasis. */
    int64 RemainingNs = 0;
};
// The same layout on every platform: a build where this fails has drifted from the others
static_assert(sizeof(Stasis) == 8);

/** The pure rule of stasis. Sandbox.Stasis calls Step once per tick; tests call it too. */
namespace StasisUtil
{
    /** How long one stasis lasts, in simulation time. */
    inline constexpr int64 DurationNs = 2'000'000'000;

    /**
     * DurationNs in whole ticks of this length, rounded to the nearest tick: 60 ticks at
     * 30 Hz, 120 at 60 Hz, 240 at 120 Hz. So a stasis covers the same simulated time at any
     * tick rate.
     */
    constexpr int64 LengthNs(const int64 t_TickNs)
    {
        const int64 Ticks = (DurationNs + t_TickNs / 2) / t_TickNs;
        return Ticks * t_TickNs;
    }

    /**
     * One tick of stasis: count the time left down by the tick, start a full stasis on a
     * press (a press during stasis starts it again), and set the policy flag. The tick that
     * starts it has already turned the camera with its own input, so the flag stops input
     * from the next tick on, for LengthNs of simulation time.
     */
    inline void Step(Stasis& t_Stasis, LookPolicy& t_Policy, const bool t_Pressed, const int64 t_TickNs)
    {
        t_Stasis.RemainingNs = std::max<int64>(t_Stasis.RemainingNs - t_TickNs, 0);
        if (t_Pressed)
        {
            t_Stasis.RemainingNs = LengthNs(t_TickNs);
        }
        t_Policy.InStasis = t_Stasis.RemainingNs > 0;
    }
} // namespace StasisUtil
