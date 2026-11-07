#pragma once

#include "Components/Transform.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Mover.h"
#include "SandboxToggles.h"

namespace Sandbox
{
    /**
     * Moves each Mover toward its target while SandboxToggles::MoveLights is on, and
     * flips the target when it gets close. Runs after Sandbox.Rotator, because both
     * write Transform.
     */
    class MoverSystem final : public Fling::System<MoverSystem,
                                                   Fling::Access<Fling::Read<Fling::SimTime>,
                                                                 Fling::Read<SandboxToggles>,
                                                                 Fling::Write<Mover>,
                                                                 Fling::Write<Fling::Transform>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{
            .Name = "Sandbox.Mover", .Phase = Fling::Phases::Simulation, .After = "Sandbox.Rotator"};

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Sandbox
