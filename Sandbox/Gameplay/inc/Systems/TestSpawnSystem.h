#pragma once

#include "Components/InputSnapshot.h"
#include "Components/Transform.h"
#include "Ecs/SystemBase.h"
#include "MeshRenderer.h"
#include "Mover.h"
#include "SandboxToggles.h"

namespace Sandbox
{
    /**
     * Spawns a test sphere 1 unit further left each time 0 (or Y) is pressed. Runs
     * before Sandbox.Rotator, so a new sphere spins on the tick it is made.
     */
    class TestSpawnSystem final : public Fling::System<TestSpawnSystem,
                                                       Fling::Access<Fling::Read<Fling::InputSnapshot>,
                                                                     Fling::Write<SandboxSpawnState>,
                                                                     Fling::Write<Fling::MeshRenderer>,
                                                                     Fling::Write<Rotator>,
                                                                     Fling::Write<Fling::Transform>,
                                                                     Fling::Spawns>>
    {
    public:
        static constexpr Fling::SystemInfo Info{
            .Name = "Sandbox.TestSpawn", .Phase = Fling::Phases::Simulation, .Before = "Sandbox.Rotator"};

        /** Each play starts the row of spheres again. */
        void OnPlayBegin(Context& ctx) const;

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Sandbox
