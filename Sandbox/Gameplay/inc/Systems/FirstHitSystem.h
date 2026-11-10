#pragma once

#include "BallControl.h"
#include "Ecs/SystemBase.h"
#include "PhysicsComponents.h"
#include "PhysicsTypes.h"

namespace Sandbox
{
    /**
     * Logs the first time that a BallControl body hits another moving body. The
     * headless physics test level looks for this line. ContactEvents is from the
     * physics step of the last frame. Owns SandboxPhysicsState, and counts ticks in it.
     */
    class FirstHitSystem final : public Fling::System<FirstHitSystem,
                                                      Fling::Access<Fling::Read<Fling::ContactEvents>,
                                                                    Fling::Read<BallControl>,
                                                                    Fling::Read<Fling::RigidBody>,
                                                                    Fling::Write<SandboxPhysicsState>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Sandbox.FirstHit", .Phase = Fling::Phases::Simulation};

        /** Each play counts ticks from 0 and logs its own first hit. */
        void OnPlayBegin(Context& ctx) const;

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Sandbox
