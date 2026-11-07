#pragma once

#include "BallControl.h"
#include "Components/InputState.h"
#include "Ecs/SystemBase.h"
#include "PhysicsTypes.h"

namespace Sandbox
{
    /**
     * Pushes and jumps each BallControl body with the player's input: the arrow keys
     * (or the D-pad) push it on the XZ plane, and Space (or A) jumps. The requests go
     * into PhysicsCommands, and physics applies them in its next step.
     */
    class BallControlSystem final : public Fling::System<BallControlSystem,
                                                         Fling::Access<Fling::Read<Fling::InputState>,
                                                                       Fling::Read<BallControl>,
                                                                       Fling::Append<Fling::PhysicsCommands>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Sandbox.BallControl", .Phase = Fling::Phases::Simulation};

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Sandbox
