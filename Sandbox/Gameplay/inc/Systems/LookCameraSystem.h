#pragma once

#include "Components/InputState.h"
#include "Ecs/SystemBase.h"
#include "LookCamera.h"

namespace Sandbox
{
    /**
     * Runs each LookCamera over the input of this tick (InputState's slice) with
     * LookUtil::Run: mouse samples turn the view, and F, Minus and Equal change the policy
     * at their place among them.
     * Another fixed system that changes the policy for another reason (Sandbox.Stasis)
     * writes LookCamera too, so it must order itself against this one, and run after it.
     */
    class LookCameraSystem final
        : public Fling::System<LookCameraSystem,
                               Fling::Access<Fling::Read<Fling::InputState>, Fling::Write<LookCamera>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Sandbox.LookCamera", .In = Fling::Order::Simulation};

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Sandbox
