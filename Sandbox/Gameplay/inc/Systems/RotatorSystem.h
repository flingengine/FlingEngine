#pragma once

#include "Components/Transform.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Mover.h"
#include "SandboxToggles.h"

namespace Sandbox
{
    /** Spins each Rotator around the Y axis while SandboxToggles::Rotate is on. */
    class RotatorSystem final : public Fling::System<RotatorSystem,
                                                     Fling::Access<Fling::Read<Fling::SimTime>,
                                                                   Fling::Read<SandboxToggles>,
                                                                   Fling::Read<Rotator>,
                                                                   Fling::Write<Fling::Transform>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Sandbox.Rotator", .Phase = Fling::Phases::Simulation};

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Sandbox
