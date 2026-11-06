#pragma once

#include "Components/Transform.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "PhysicsComponents.h"
#include "PhysicsTypes.h"

namespace Fling
{
    /**
     * The physics phase, once per fixed tick during play: Physics::Tick with the tick's
     * fixed delta. It is a bridge to the physics subsystem, which works on the storage
     * itself (RawStorage), so its access list names every type that Physics::Tick reads
     * or writes. It consumes PhysicsCommands, so every system that appends a request
     * runs before it, and it writes ContactEvents, which gameplay reads on the next tick.
     */
    class PhysicsStepSystem final : public System<PhysicsStepSystem,
                                                  Access<Read<SimTime>,
                                                         Read<Collider>,
                                                         Write<Transform>,
                                                         Write<RigidBody>,
                                                         Write<PhysicsBody>,
                                                         Write<PhysicsWorld>,
                                                         Write<PhysicsSettings>,
                                                         Write<ContactEvents>,
                                                         Consume<PhysicsCommands>,
                                                         RawStorage>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Physics.Step", .Phase = Phases::ResolveDeferred};

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Fling
