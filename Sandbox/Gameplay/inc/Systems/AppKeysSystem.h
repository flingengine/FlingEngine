#pragma once

#include "Components/AppRequests.h"
#include "Components/InputState.h"
#include "Ecs/SystemBase.h"

namespace Sandbox
{
    /**
     * Turns the application keys into AppRequests: Escape (or View) quits, and M
     * (or Menu) shows or hides the mouse cursor. In editor play, quit stops play.
     */
    class AppKeysSystem final
        : public Fling::System<AppKeysSystem,
                               Fling::Access<Fling::Read<Fling::InputState>, Fling::Append<Fling::AppRequests>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Sandbox.AppKeys", .Phase = Fling::Phases::Simulation};

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Sandbox
