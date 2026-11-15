#pragma once

#include "Components/InputState.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "LookCamera.h"
#include "SandboxSystemsFwd.h"
#include "Stasis.h"

namespace Sandbox
{
    /**
     * A press of G puts each camera with a Stasis in stasis for StasisUtil::DurationNs of
     * simulation time (StasisUtil::Step): it counts down and sets LookCamera::Policy.InStasis,
     * which makes the look rule drop mouse input on both sides.
     *
     * It writes LookCamera, as Sandbox.LookCamera does, so the two must be ordered. It runs
     * after: the tick that sets the flag has already used its own input under the old
     * policy, and the frame side, which cannot predict a gameplay decision, drops pending
     * input from the first frame that shows that tick. Before Sandbox.LookCamera, the tick
     * would drop input that the frame side had already shown (Docs/input-two-sided.md, section 7).
     */
    class StasisSystem final : public Fling::System<StasisSystem,
                                                    Fling::Access<Fling::Read<Fling::InputState>,
                                                                  Fling::Read<Fling::SimTime>,
                                                                  Fling::Write<Stasis>,
                                                                  Fling::Write<LookCamera>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{
            .Name = "Sandbox.Stasis", .In = Fling::Order::Simulation, .After = Fling::SystemOf<LookCameraSystem>};

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Sandbox
