#pragma once

#include "BallControl.h"
#include "Components/InputState.h"
#include "Ecs/SystemBase.h"
#include "SandboxToggles.h"

namespace Sandbox
{
    /**
     * Flips the SandboxToggles from this tick's input: T (or the X button) for the
     * Rotator spin, and Space (or the A button) for the Mover lights. Space jumps
     * instead when the level has a ball, so it does not toggle the lights then.
     */
    class TogglesSystem final
        : public Fling::System<
              TogglesSystem,
              Fling::Access<Fling::Read<Fling::InputState>, Fling::Read<BallControl>, Fling::Write<SandboxToggles>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Sandbox.Toggles", .Phase = Fling::Phases::Simulation};

        /** Each play starts with both toggles off. */
        void OnPlayBegin(Context& ctx) const;

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Sandbox
